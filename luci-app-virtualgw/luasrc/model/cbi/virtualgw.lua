local m, s, o

-- 确保必要的依赖存在
local sys = require "luci.sys"
local nixio = require "nixio"
local util = require "luci.util"
local uci = require "luci.model.uci".cursor()
local i18n = require "luci.i18n"

-- 确保全局配置存在
uci:foreach("virtualgw", "global", function() return end)
if uci:get("virtualgw", "global") == nil then
    uci:section("virtualgw", "global", "global", {
        enabled = "0",
        state = "master",
        detect_src_addr = "8.8.8.8",
        check_interval = "3",
        log_level = "1"
    })
    uci:commit("virtualgw")
end

-- 加载翻译目录
i18n.loadc("virtualgw")

m = Map("virtualgw", translate("Virtual Gateway"), translate("Configure virtual gateway settings"))

-- 全局配置
s = m:section(NamedSection, "global", "global", translate("Global Settings"))
s.anonymous = true
s.addremove = false

o = s:option(Flag, "enabled", translate("Enable"))
o.rmempty = false
o.default = "0"
o.description = translate("Enable or disable Virtual Gateway service")

o = s:option(ListValue, "state", translate("Working Mode"))
o:value("master", translate("Master - Primary Router"))
o:value("side", translate("Side - Secondary Router"))
o.rmempty = false
o.description = translate("Master mode runs on primary router, Side mode on secondary router")

o = s:option(Value, "detect_src_addr", translate("Detection Address"))
o.rmempty = false
o.placeholder = "8.8.8.8"
o.datatype = "host"
o.description = translate("For master mode, specify the side router IP. For side mode, specify the external IP to detect.")

o = s:option(Value, "check_interval", translate("Check Interval (seconds)"))
o.rmempty = false
o.default = "3"
o.datatype = "uinteger"
o.description = translate("Interval in seconds between connectivity checks")

o = s:option(ListValue, "log_level", translate("Log Level"))
o:value("0", translate("Disabled"))
o:value("1", translate("Basic"))
o:value("2", translate("Verbose"))
o.rmempty = false
o.default = "1"
o.description = translate("Set logging detail level")

-- 虚拟网关接口配置
s = m:section(TypedSection, "virtual_gw", translate("Interface Settings"))
s.anonymous = true
s.addremove = false

-- 获取网络接口列表
local net_devices = {}
for _, dev in pairs(sys.net.devices()) do
    if dev:match("^[^.]+$") and not dev:match("^lo$") then
        net_devices[#net_devices+1] = dev
    end
end

-- 添加bridge接口
local bridges = {}
uci:foreach("network", "interface", function(s)
    local name = s[".name"]
    local device = s.device or s.ifname
    if device and device:match("^br-%S+$") then
        bridges[#bridges+1] = device
    end
end)

for _, br in ipairs(bridges) do
    if not util.contains(net_devices, br) then
        net_devices[#net_devices+1] = br
    end
end

table.sort(net_devices)

o = s:option(ListValue, "device", translate("Physical Device"))
for _, dev in ipairs(net_devices) do
    o:value(dev)
end
o.rmempty = false
o.description = translate("The physical network device to attach to")

o = s:option(Value, "ipaddr", translate("IP Address"))
o.rmempty = false
o.datatype = "ip4addr"
o.description = translate("Virtual interface IP address")

o = s:option(Value, "netmask", translate("Netmask"))
o.rmempty = false
o.datatype = "ip4addr"
o.description = translate("Virtual interface netmask")
o.default = "255.255.255.0"

return m