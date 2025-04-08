include $(TOPDIR)/rules.mk

PKG_NAME:=virtualgw
PKG_VERSION:=2.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/virtualgw
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Virtual Gateway Core
  DEPENDS:=+libubox +libubus +libuci
endef

define Package/virtualgw/install
    $(INSTALL_DIR) $(1)/usr/bin
    $(INSTALL_BIN) $(PKG_BUILD_DIR)/virtualgw $(1)/usr/bin/
    $(INSTALL_DIR) $(1)/etc/config
    $(INSTALL_CONF) ./files/virtualgw.conf $(1)/etc/config/
    $(INSTALL_DIR) $(1)/etc/init.d
    $(INSTALL_BIN) ./files/virtualgw.init $(1)/etc/init.d/
endef

define Build/Compile
    $(TARGET_CC) $(TARGET_CFLAGS) -o $(PKG_BUILD_DIR)/virtualgw \
        src/main.c src/config.c src/network.c src/ubus.c
endef

$(eval $(call BuildPackage,virtualgw))