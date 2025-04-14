# Copyright (C) 2023
#
# This is free software, licensed under the Apache License, Version 2.0 .
#

include $(TOPDIR)/rules.mk

LUCI_TITLE:=LuCI support for Virtual Gateway
LUCI_DEPENDS:=+virtualgw +luci-base +luci-compat
LUCI_PKGARCH:=all
PKG_VERSION:=1.0.0
# PKG_RELEASE MUST be empty for luci.mk
PKG_RELEASE:=

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature

