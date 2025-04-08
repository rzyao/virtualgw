include $(TOPDIR)/rules.mk

PKG_NAME:=virtualgw
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_MAINTAINER:=Your Name <your.email@example.com>
PKG_LICENSE:=GPL-3.0

include $(INCLUDE_DIR)/package.mk

define Package/virtualgw
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Virtual Gateway Service
  DEPENDS:=+libopenssl +libpthread +librt
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
	$(TARGET_CC) $(TARGET_CFLAGS) -o $(PKG_BUILD_DIR)/virtualgw \
		src/main.c src/config.c src/network.c src/ubus.c
endef

define Package/virtualgw/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/virtualgw $(1)/usr/bin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./src/files/virtualgw.init $(1)/etc/init.d/virtualgw
endef

$(eval $(call BuildPackage,virtualgw))
