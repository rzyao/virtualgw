module("luci.controller.virtualgw", package.seeall)

function index()
    if not nixio.fs.access("/etc/config/virtualgw") then
        return
    end
    
    entry({"admin", "services", "virtualgw"}, cbi("virtualgw"), _("Virtual Gateway"), 10).dependent = true
end
