#include <stdio.h>
#include <uci.h>
#include <sys/wait.h>
#include <syslog.h>
#include <stdlib.h>
#include "config.h"
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>    // for sleep()
#include "status.h"
/**
 * 配置虚拟网关网络接口
 * @return 状态码（0=成功，负数=错误码）
 * 
 * 功能流程：
 * 1. 创建UCI上下文
 * 2. 加载network配置包
 * 3. 检查/创建virtual_gw接口段
 * 4. 设置静态网络参数
 * 5. 提交配置变更
 * 
 * 错误码说明：
 * -1: UCI配置加载失败
 * -2: 接口段创建失败
 * -3: 参数设置失败
 * -4: 配置提交失败
 */
int configure_network_interface(const struct config *cfg) {
    syslog(LOG_INFO, "[Network] 配置网络接口");
    struct uci_context *ctx = uci_alloc_context(); // UCI配置上下文
    struct uci_package *pkg = NULL;                // network配置包指针
    int ret = 0;                                   // 返回值初始化

    // 加载network配置（路径为/etc/config/network）
    if (uci_load(ctx, "network", &pkg) != UCI_OK) {
        syslog(LOG_ERR, "[Network] network配置读取失败");
        uci_free_context(ctx);
        return -1; // 错误码-1：配置加载失败
    }
    syslog(LOG_INFO, "[Network] network配置读取成功");

    // 检查virtual_gw接口段是否存在
    struct uci_section *sec = uci_lookup_section(ctx, pkg, "virtual_gw");
    if (!sec) {
        syslog(LOG_INFO, "virtual_gw接口不存在，创建virtual_gw接口");
        struct uci_section *interface_sec;

        // 创建新接口段（类型为interface）
        if (uci_add_section(ctx, pkg, "interface", &interface_sec) != UCI_OK) {
            syslog(LOG_ERR, "[Network] 接口创建失败");
            ret = -2;
            goto cleanup;
        }

        // 新增段重命名操作（关键修复）
        struct uci_ptr rename_ptr = {
            .package = "network",
            .section = interface_sec->e.name,
            .value = "virtual_gw"
        };
        if (uci_rename(ctx, &rename_ptr) != UCI_OK) {
            syslog(LOG_ERR, "[Network] 接口重命名失败");
            ret = -3;
            goto cleanup;
        }
    }
    

    /* 静态网络参数配置表
     * 格式：选项名，选项值，以NULL结尾
     * 包含协议类型、设备名称、IP地址等关键参数
     */
    const char *options[] = {
        "device",   cfg->interface.device,       // 绑定物理网卡
        "proto",    "static",     // 使用静态IP协议
        "ipaddr",   cfg->interface.ipaddr, // 虚拟网关IP
        "netmask",  cfg->interface.netmask, // 子网掩码
        "auto",     "0", // 止该网络接口在系统启动或网络服务重启时自动启用
        NULL // 结束标记
    };

    // 修改参数设置循环，使用验证后的sec指针
    syslog(LOG_INFO, "[Network] 开始设置接口参数");
    for (int i = 0; options[i]; i += 2) {
        syslog(LOG_INFO, "[Network] 设置参数：%s=%s", options[i], options[i+1]);
        struct uci_ptr param_ptr = {
            .package = "network",
            .section = "virtual_gw",  // 使用实际的段名称
            .option = options[i],
            .value = options[i+1]
        };
        
        if (uci_set(ctx, &param_ptr) != UCI_OK) {
            syslog(LOG_ERR, "[Network] 参数设置失败");
            ret = -3;
            goto cleanup;
        }
    }
    syslog(LOG_INFO, "[Network] 接口参数设置成功");

    // 提交配置变更到持久化存储
    if (uci_commit(ctx, &pkg, false) != UCI_OK) {
        syslog(LOG_ERR, "[Network] 配置配置保存失败");
        ret = -4;
        goto cleanup;
    }
    syslog(LOG_INFO, "[Network] 接口配置保存成功");

cleanup:
    // 资源清理（逆序释放）
    uci_unload(ctx, pkg);  // 卸载配置包
    uci_free_context(ctx); // 释放上下文
    return ret;
}


/**
 * 启用网络接口
 * @param ifname 接口名称（如"virtual_gw"）
 * @return 状态码（0=成功，负数=错误码）
 * 
 */
int enable_network_interface(const char *ifname) {
    if (gw_status != 0) {
        int count = 0;
        system("ifup virtual_gw");
        while (is_gw_up(ifname) != 0)
        {
            sleep(1);
            count++;
            if (count > 10) {
                syslog(LOG_ERR, "[Network] 接口启动失败");
                return -1;
            }
        }
        syslog(LOG_ERR, "[Network] 接口启动成功");
        gw_status = 0;
    }
    return 0;
}

/**
 * 设置网络接口禁用
 * @param ifname 接口名称（如"virtual_gw"）
 * @return 状态码（0=成功，负数=错误码）
 * 
 */
int disable_network_interface(const char *ifname) {
    if (gw_status != 1) {
        system("ifdown virtual_gw");
        
        // 添加验证逻辑
        int count = 0;
        while (is_gw_up(ifname) == 0) {
            sleep(1);
            count++;
            if (count > 10) {
                syslog(LOG_ERR, "[Network] 接口关闭失败");
                break;  // 即使失败也继续
            }
        }
        gw_status = 1;
    }
    return 0;
}

/**
 * 禁用LAN接口的ping响应
 * @return 状态码（0=成功，非0=失败）
 */
int disable_ping_response() {
    int ret = 0;
    
    // 检查规则是否已存在
    ret = system("uci -q get firewall.virtual_gw_lan_offline >/dev/null 2>&1");
    
    if (ret == 0) {
        // 规则已存在，删除启用标志
        system("uci -q delete firewall.virtual_gw_lan_offline.enabled >/dev/null 2>&1");
    } else {
        // 创建新的防火墙规则
        system("uci -q batch <<-EOF >/dev/null 2>&1\n"
               "set firewall.virtual_gw_lan_offline=rule\n"
               "set firewall.virtual_gw_lan_offline.name=Virtual-Gateway-LAN-Offline\n"
               "set firewall.virtual_gw_lan_offline.src=lan\n"
               "set firewall.virtual_gw_lan_offline.proto=icmp\n"
               "set firewall.virtual_gw_lan_offline.icmp_type=echo-request\n"
               "set firewall.virtual_gw_lan_offline.family=ipv4\n"
               "set firewall.virtual_gw_lan_offline.target=DROP\n"
               "EOF");
    }
    
    // 提交配置并重载防火墙
    system("uci commit firewall");
    system("/etc/init.d/firewall reload >/dev/null 2>&1");
    
    return 0;
}

/**
 * 启用LAN接口的ping响应
 * @return 状态码（0=成功，非0=失败）
 */
int enable_ping_response() {
    // 设置规则为禁用状态（UCI中0=禁用规则，相当于允许ping）
    system("uci -q set firewall.virtual_gw_lan_offline.enabled=0 >/dev/null 2>&1");
    
    // 提交配置并重载防火墙
    system("uci commit firewall");
    system("/etc/init.d/firewall reload >/dev/null 2>&1");
    
    return 0;
}





   