#include <stdio.h>
#include <uci.h>
#include <sys/wait.h>
#include <syslog.h>

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
int configure_network_interface(const struct detector_config *cfg) {
    struct uci_context *ctx = uci_alloc_context(); // UCI配置上下文
    struct uci_package *pkg = NULL;                // network配置包指针
    int ret = 0;                                   // 返回值初始化

    // 加载network配置（路径为/etc/config/network）
    if (uci_load(ctx, "network", &pkg) != UCI_OK) {
        uci_free_context(ctx);
        return -1; // 错误码-1：配置加载失败
    }

    // 准备接口配置操作指针
    struct uci_ptr ptr = {
        .package = "network",      // 操作目标配置包
        .section = "virtual_gw",   // 目标配置段名称
        .option = NULL,            // 动态设置选项名
        .value = NULL              // 动态设置选项值
    };

    // 检查virtual_gw接口段是否存在
    if (!uci_lookup_section(ctx, pkg, "virtual_gw")) {
        struct uci_section *interface_sec;
        
        // 创建新接口段（类型为interface）
        if (uci_add_section(ctx, pkg, "interface", &interface_sec) != UCI_OK) {
            ret = -2; // 错误码-2：接口创建失败
            goto cleanup;
        }
        
        // 重命名新创建的段为virtual_gw
        uci_rename_section(ctx, interface_sec, "virtual_gw");
    }

    /* 静态网络参数配置表
     * 格式：选项名，选项值，以NULL结尾
     * 包含协议类型、设备名称、IP地址等关键参数
     */
    const char *options[] = {
        "proto",    "static",     // 使用静态IP协议
        "device",   "eth0",       // 绑定物理网卡
        "ipaddr",   "192.168.50.5", // 虚拟网关IP
        "netmask",  "255.255.255.0", // 子网掩码
        "gateway",  "192.168.50.1",  // 默认网关
        NULL // 结束标记
    };

    // 遍历配置表设置参数
    for (int i = 0; options[i]; i += 2) {
        ptr.option = options[i];   // 设置当前选项名
        ptr.value = options[i+1];  // 设置当前选项值
        
        if (uci_set(ctx, &ptr) != UCI_OK) {
            ret = -3; // 错误码-3：参数设置失败
            goto cleanup;
        }
    }

    // 提交配置变更到持久化存储
    if (uci_commit(ctx, &pkg, false) != UCI_OK) {
        ret = -4; // 错误码-4：提交失败
    }

cleanup:
    // 资源清理（逆序释放）
    uci_unload(ctx, pkg);  // 卸载配置包
    uci_free_context(ctx); // 释放上下文
    return ret;
}

/**
 * 设置网络接口启用状态
 * @param ifname 接口名称（如"virtual_gw"）
 * @param enable 启用标志（1=启用，0=禁用）
 * @return 状态码（0=成功，负数=错误码）
 * 
 * 实现原理：
 * 通过设置/删除"disabled"选项控制接口状态：
 * - 启用：删除disabled选项
 * - 禁用：设置disabled=1
 */
int set_interface_status(const char *ifname, int enable) {
    struct uci_context *ctx = uci_alloc_context();
    struct uci_ptr ptr = {
        .package = "network",    // 操作network配置
        .section = ifname,       // 目标接口段
        .option = "disabled",    // 操作目标选项
        .value = enable ? NULL : "1" // 禁用时设为"1"
    };

    int ret = 0;
    
    // 查找配置指针
    if (uci_lookup_ptr(ctx, &ptr, NULL, true) != UCI_OK) {
        ret = -1; // 错误码-1：配置查找失败
        goto cleanup;
    }

    if (enable) {
        // 启用接口：删除disabled选项
        if (uci_delete(ctx, &ptr) != UCI_OK) {
            ret = -2; // 错误码-2：删除操作失败
        }
    } else {
        // 禁用接口：设置disabled=1
        if (uci_set(ctx, &ptr) != UCI_OK) {
            ret = -3; // 错误码-3：设置操作失败
        }
    }

    // 提交变更（仅在操作成功时）
    if (ret == 0) {
        if (uci_commit(ctx, &ptr.p, false) != UCI_OK) {
            ret = -4; // 错误码-4：提交失败
        }
    }

cleanup:
    uci_free_context(ctx);
    return ret;
}

/**
 * 安全执行ubus调用（带超时和错误检查）
 * @param service 服务名称（如"network"）
 * @param method  方法名称（如"reload"）
 * @return 状态码（0=成功，-1=执行失败）
 * 
 * 实现特点：
 * - 使用system执行命令
 * - 超时5秒（-t 5参数）
 * - 检查子进程退出状态
 * - 记录详细错误日志
 */
int safe_ubus_call(const char *service, const char *method) {
    char cmd[128];
    // 构造ubus命令（格式：ubus -t 5 call 服务 方法）
    snprintf(cmd, sizeof(cmd), "ubus -t 5 call %s %s", service, method);
    
    int ret = system(cmd); // 执行系统命令
    
    // 解析子进程退出状态
    if (WIFEXITED(ret)) {
        int exit_code = WEXITSTATUS(ret);
        if (exit_code != 0) {
            syslog(LOG_ERR, "%s.%s失败，错误码：%d", 
                  service, method, exit_code);
            return -1;
        }
    }
    return 0;
}

/**
 * 重新加载网络配置
 * @return ubus调用结果
 */
int reload_network_config() {
    return safe_ubus_call("network", "reload");
}

/**
 * 重新加载防火墙配置
 * @return ubus调用结果
 */
int reload_firewall_config() {
    return safe_ubus_call("firewall", "reload");
}

/**
 * 启用虚拟接口（组合操作）
 * @return 状态码（0=成功，负数=错误码）
 * 
 * 执行流程：
 * 1. 启用网络接口（删除disabled标记）
 * 2. 重载网络配置
 * 3. 重载防火墙规则
 */
int enable_virtual_interface() {
    int ret = set_interface_status("virtual_gw", 1);
    if (ret == 0) {
        // 顺序执行重载操作（网络->防火墙）
        reload_network_config();
        reload_firewall_config();
    }
    return ret;
}

/**
 * 禁用虚拟接口（组合操作）
 * @return 状态码（0=成功，负数=错误码）
 * 
 * 执行流程：
 * 1. 禁用网络接口（设置disabled=1）
 * 2. 重载网络配置
 * 3. 重载防火墙规则
 */
int disable_virtual_interface() {
    int ret = set_interface_status("virtual_gw", 0);
    if (ret == 0) {
        reload_network_config();
        reload_firewall_config();
    }
    return ret;
}
