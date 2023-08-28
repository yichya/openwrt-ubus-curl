cmake_minimum_required(VERSION 2.6)

PROJECT(curl-ubus C)
ADD_DEFINITIONS(-Os -Wall -Werror --std=gnu99 -g3 -Wmissing-declarations)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

FIND_LIBRARY(curl_library NAMES curl) 
FIND_PATH(curl_include_dir curl/curl.h)
INCLUDE_DIRECTORIES(${curl_include_dir})

FIND_LIBRARY(ubox_library NAMES ubox)
FIND_PATH(ubox_include_dir libubox/usock.h)
INCLUDE_DIRECTORIES(${ubox_include_dir})

FIND_LIBRARY(blob_library NAMES blobmsg_json)
FIND_LIBRARY(ubus_library NAMES ubus)

ADD_EXECUTABLE(curl-ubus server.c)
TARGET_LINK_LIBRARIES(curl-ubus ${ubus_library} ${ubox_library} ${blob_library} ${curl_library})

INSTALL(TARGETS curl-ubus
	RUNTIME DESTINATION sbin
)
