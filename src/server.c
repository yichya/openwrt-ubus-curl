#include <unistd.h>
#include <signal.h>

#include <curl/curl.h>
#include <libubox/blobmsg_json.h>
#include <libubus.h>

static struct ubus_context *ctx;
static struct blob_buf b;

struct memory_struct
{
    char *memory;
    size_t size;
};

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct memory_struct *mem = (struct memory_struct *)userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL)
    {
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

enum
{
    CURL_UBUS_GET_ID,
    CURL_UBUS_GET_URL,
    __CURL_UBUS_GET_MAX
};

static const struct blobmsg_policy curl_ubus_get_policy[] = {
    [CURL_UBUS_GET_ID] = {.name = "id", .type = BLOBMSG_TYPE_INT32},
    [CURL_UBUS_GET_URL] = {.name = "url", .type = BLOBMSG_TYPE_STRING},
};

struct curl_ubus_get_request
{
    struct ubus_request_data req;
    struct uloop_timeout timeout;
    int fd;
    int idx;
    char data[];
};

static void curl_ubus_get_fd_reply(struct uloop_timeout *t)
{
    struct curl_ubus_get_request *req = container_of(t, struct curl_ubus_get_request, timeout);
    char *data;

    data = alloca(strlen(req->data) + 32);
    sprintf(data, "msg%d: %s\n", ++req->idx, req->data);
    if (write(req->fd, data, strlen(data)) < 0)
    {
        close(req->fd);
        free(req);
        return;
    }

    uloop_timeout_set(&req->timeout, 1000);
}

static void curl_ubus_get_reply(struct uloop_timeout *t)
{
    struct curl_ubus_get_request *req = container_of(t, struct curl_ubus_get_request, timeout);
    int fds[2];

    blob_buf_init(&b, 0);
    blobmsg_add_json_from_string(&b, req->data);
    ubus_send_reply(ctx, &req->req, b.head);

    if (pipe(fds) == -1)
    {
        return;
    }
    ubus_request_set_fd(ctx, &req->req, fds[0]);
    ubus_complete_deferred_request(ctx, &req->req, 0);
    req->fd = fds[1];

    req->timeout.cb = curl_ubus_get_fd_reply;
    curl_ubus_get_fd_reply(t);
}

static int curl_ubus_get(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
    struct curl_ubus_get_request *hreq;
    struct blob_attr *tb[__CURL_UBUS_GET_MAX];
    const char *url = "";

    blobmsg_parse(curl_ubus_get_policy, ARRAY_SIZE(curl_ubus_get_policy), tb, blob_data(msg), blob_len(msg));
    if (tb[CURL_UBUS_GET_URL])
    {
        url = blobmsg_data(tb[CURL_UBUS_GET_URL]);
    }
    else
    {
        return UBUS_STATUS_UNKNOWN_ERROR;
    }

    struct memory_struct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    CURL *curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

    CURLcode res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK)
    {
        curl_easy_cleanup(curl_handle);
        free(chunk.memory);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }

    size_t len = sizeof(*hreq) + chunk.size + 1;
    hreq = calloc(1, len);
    if (!hreq)
    {
        curl_easy_cleanup(curl_handle);
        free(chunk.memory);
        return UBUS_STATUS_UNKNOWN_ERROR;
    }

    snprintf(hreq->data, len, "%s", chunk.memory);

    curl_easy_cleanup(curl_handle);
    free(chunk.memory);

    ubus_defer_request(ctx, req, &hreq->req);
    hreq->timeout.cb = curl_ubus_get_reply;
    uloop_timeout_set(&hreq->timeout, 1);

    return 0;
}

static const struct ubus_method curl_ubus_methods[] = {
    UBUS_METHOD("get", curl_ubus_get, curl_ubus_get_policy),
};

static struct ubus_object_type curl_ubus_object_type = UBUS_OBJECT_TYPE("curl", curl_ubus_methods);

static struct ubus_object curl_ubus_object = {
    .name = "curl",
    .type = &curl_ubus_object_type,
    .methods = curl_ubus_methods,
    .n_methods = ARRAY_SIZE(curl_ubus_methods),
};

static void server_main(void)
{
    int ret = ubus_add_object(ctx, &curl_ubus_object);
    if (ret)
    {
        return;
    }

    uloop_run();
}

int main(int argc, char **argv)
{
    const char *ubus_socket = NULL;
    int ch;

    while ((ch = getopt(argc, argv, "cs:")) != -1)
    {
        switch (ch)
        {
        case 's':
            ubus_socket = optarg;
            break;
        default:
            break;
        }
    }

    uloop_init();
    signal(SIGPIPE, SIG_IGN);

    ctx = ubus_connect(ubus_socket);
    if (!ctx)
    {
        return -1;
    }

    ubus_add_uloop(ctx);

    curl_global_init(CURL_GLOBAL_ALL);
    server_main();
    curl_global_cleanup();

    ubus_free(ctx);
    uloop_done();
    return 0;
}
