#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>


/**
 * 加载并解析完整配置
 * @param cfg 配置结构体指针（需预先分配内存）
 * @return 错误码（CONFIG_ERR_OK表示成功）
 * 
 * 执行流程：
 * 1. 初始化配置结构体
 * 2. 加载UCI配置包
 * 3. 按顺序解析各配置段
 * 4. 错误处理和资源释放
 */
config_error_t config_load(struct config *cfg) {
    syslog(LOG_INFO, "[Config] 执行config_load");
    struct uci_context *ctx = uci_alloc_context();
    struct uci_package *pkg = NULL;
    int res = CONFIG_ERR_OK;

    // 初始化配置结构体
    memset(cfg, 0, sizeof(struct config));

    // 加载配置文件
    if (uci_load(ctx, "virtualgw", &pkg)) {
        syslog(LOG_ERR, "[Config] 配置读取失败");
        res = CONFIG_ERR_UCI_LOAD;
        goto cleanup;
    }
    syslog(LOG_INFO, "[Config] 配置读取成功");

    //--------------------- 解析global段 ---------------------
    struct uci_section *global_sec = uci_lookup_section(ctx, pkg, "global");
    if (!global_sec) {
        syslog(LOG_ERR, "[Config] 缺少global段，请检查配置文件结构");
        res = CONFIG_ERR_MISSING_SECTION;
        goto cleanup;
    }

    cfg->global.log_level = uci_get_int_default(ctx, global_sec, "log_level", 1);
    
    const char *state = uci_lookup_option_string(ctx, global_sec, "state");
    if (!state) {
        syslog(LOG_ERR, "[Config] global段缺少state参数");
        res = CONFIG_ERR_INVALID_VALUE;
        goto cleanup;
    }
    strncpy(cfg->global.state, state, sizeof(cfg->global.state) - 1);
    
    // 验证state值
    if (strcmp(cfg->global.state, "master") != 0 && 
        strcmp(cfg->global.state, "side") != 0) {
        syslog(LOG_ERR, "[Config] state值必须为master或side");
        res = CONFIG_ERR_INVALID_VALUE;
        goto cleanup;
    }
    
    const char *detect_src_addr = uci_lookup_option_string(ctx, global_sec, "detect_src_addr");
    if (detect_src_addr) {
        strncpy(cfg->global.detect_src_addr, detect_src_addr, MAX_IP_LEN - 1);
    } else {
        strncpy(cfg->global.detect_src_addr, "www.baidu.com", MAX_IP_LEN - 1);
    }
    
    cfg->global.check_interval = uci_get_int_default(ctx, global_sec, "check_interval", 2);

    //------------------- 解析interface段 --------------------
    struct uci_section *if_sec = uci_lookup_section(ctx, pkg, "virtual_gw");
    if (!if_sec) {
        syslog(LOG_ERR, "[Config] 缺少virtual_gw接口段，请检查配置文件");
        res = CONFIG_ERR_MISSING_SECTION;
        goto cleanup;
    }
    
    const char *device = uci_lookup_option_string(ctx, if_sec, "device");
    if (!device || strlen(device) == 0) {
        syslog(LOG_ERR, "[Config] virtual_gw段缺少device参数");
        res = CONFIG_ERR_INVALID_VALUE;
        goto cleanup;
    }
    strncpy(cfg->interface.device, device, MAX_DEVICE_LEN - 1);
    
    const char *ipaddr = uci_lookup_option_string(ctx, if_sec, "ipaddr");
    if (!ipaddr || strlen(ipaddr) == 0) {
        syslog(LOG_ERR, "[Config] virtual_gw段缺少ipaddr参数");
        res = CONFIG_ERR_INVALID_VALUE;
        goto cleanup;
    }
    strncpy(cfg->interface.ipaddr, ipaddr, MAX_IP_LEN - 1);
    
    const char *netmask = uci_lookup_option_string(ctx, if_sec, "netmask");
    if (!netmask || strlen(netmask) == 0) {
        syslog(LOG_ERR, "[Config] virtual_gw段缺少netmask参数");
        res = CONFIG_ERR_INVALID_VALUE;
        goto cleanup;
    }
    strncpy(cfg->interface.netmask, netmask, MAX_IP_LEN - 1);

cleanup:
    if (pkg) uci_unload(ctx, pkg);
    uci_free_context(ctx);
    
    if (res != CONFIG_ERR_OK) {
        syslog(LOG_DEBUG, "配置加载失败，错误码：%d", res);
    }
    return res;
}

int uci_get_int_default(struct uci_context *ctx, struct uci_section *s,
                       const char *option, int def) {
    const char *val = uci_lookup_option_string(ctx, s, option);
    return val ? atoi(val) : def;
}

int uci_get_bool_default(struct uci_context *ctx, struct uci_section *s,
                        const char *option, int def) {
    const char *val = uci_lookup_option_string(ctx, s, option);
    if (!val) return def;
    return (strcasecmp(val, "1") == 0 || strcasecmp(val, "true") == 0);
}