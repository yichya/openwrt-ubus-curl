include $(TOPDIR)/rules.mk

PKG_NAME:=openwrt-ubus-curl
PKG_VERSION:=0.1.1
PKG_RELEASE:=1

PKG_LICENSE:=MPLv2
PKG_LICENSE_FILES:=LICENSE
PKG_MAINTAINER:=yichya <mail@yichya.dev>
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/$(PKG_NAME)
	SECTION:=Custom
	CATEGORY:=Extra packages
	TITLE:=openwrt-ubus-curl
	DEPENDS:=+libubox +libubus +libblobmsg-json +curl
endef

define Package/$(PKG_NAME)/description
	Invoke curl requests via ubus
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./curl-ubus.init $(1)/etc/init.d/curl-ubus
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/curl-ubus $(1)/usr/sbin/curl-ubus
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
