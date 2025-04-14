include $(TOPDIR)/rules.mk

PKG_NAME:=virtualgw
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_FORMAT:=ipk

PKG_MAINTAINER:=Your Name <your.email@example.com>
PKG_LICENSE:=GPL-3.0

include $(INCLUDE_DIR)/package.mk

define Package/virtualgw
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Virtual Gateway Service
  DEPENDS:= +libubus +libubox +libblobmsg-json +libuci +libjson-c +libnl-tiny
  URL:=https://github.com/rzyao/virtualgw
endef

define Package/virtualgw/description
  Advanced virtual gateway implementation for OpenWrt.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
	true
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) -g -O0 -Wall -rdynamic -o $(PKG_BUILD_DIR)/virtualgw \
		$(PKG_BUILD_DIR)/*.c \
		-luci -lubox -lubus -lblobmsg_json \
		-ljson-c \
		-lpthread
endef

define Package/virtualgw/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/virtualgw $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/virtualgw.init $(1)/etc/init.d/virtualgw
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/virtualgw.config $(1)/etc/config/virtualgw
endef

LIBS := -lubus -lubox -lblobmsg_json -luci -ljson-c -lpthread

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include -I$(STAGING_DIR)/include -I./include
TARGET_LDFLAGS += -L$(STAGING_DIR)/usr/lib -Wl,-rpath-link=$(STAGING_DIR)/usr/lib

$(eval $(call BuildPackage,virtualgw))