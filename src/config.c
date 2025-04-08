#include "config.h"
#include <string.h>
#include <stdlib.h>

/**
 * 加载并解析配置文件
 * @param cfg 配置结构体指针，用于存储解析结果
 * @return 错误码（CONFIG_OK表示成功，其他为错误码）
 * 
 * 执行流程：
 * 1. 创建UCI上下文
 * 2. 加载virtualgw配置包
 * 3. 解析global段基础配置
 * 4. 遍历所有target段获取目标配置
 * 5. 资源清理和错误处理
 */
int config_load(struct detector_config *cfg) {
    struct uci_context *ctx = uci_alloc_context(); // UCI配置上下文
    struct uci_package *pkg = NULL;                // 配置包指针
    int ret = CONFIG_OK;                           // 返回值初始化

    // 加载配置文件（配置文件路径为/etc/config/virtualgw）
    if (uci_load(ctx, "virtualgw", &pkg)) {
        syslog(LOG_ERR, "[Config] Failed to load config package");
        ret = ERR_UCI_LOAD;
        goto cleanup; // 使用goto统一处理资源释放
    }

    // 解析global段配置（必须存在）
    struct uci_section *global_sec = uci_lookup_section(ctx, pkg, "global");
    if (!global_sec) {
        syslog(LOG_ERR, "[Config] Missing global section");
        ret = ERR_MISSING_SECTION;
        goto cleanup;
    }

    /* 解析全局配置项（带默认值）：
     * interval   - 检测间隔（秒），默认1
     * retry      - 重试次数，默认3
     * log_level  - 日志级别，默认1
     */
    const char *interval = uci_lookup_option_string(ctx, global_sec, "interval");
    cfg->interval = interval ? atoi(interval) : 1; // 注意：未做范围校验

    const char *retry = uci_lookup_option_string(ctx, global_sec, "retry");
    cfg->retry_count = retry ? atoi(retry) : 3;

    const char *log_level = uci_lookup_option_string(ctx, global_sec, "log_level");
    cfg->log_level = log_level ? atoi(log_level) : 1;

    // 遍历所有配置段查找target段（允许多个target）
    struct uci_element *e;
    uci_foreach_element(&pkg->sections, e) {
        struct uci_section *s = uci_to_section(e);
        if (strcmp(s->type, "target") == 0) {
            // 解析IP地址（需注意strncpy不会自动添加终止符）
            const char *ip = uci_lookup_option_string(ctx, s, "ipaddr");
            if (ip) {
                strncpy(cfg->ip, ip, MAX_IP_LEN-1);
                cfg->ip[MAX_IP_LEN-1] = '\0'; // 手动确保终止符
            }

            // 解析协议类型（tcp/udp/icmp）
            const char *proto = uci_lookup_option_string(ctx, s, "proto");
            if (proto) {
                strncpy(cfg->proto, proto, MAX_PROTO_LEN-1);
                cfg->proto[MAX_PROTO_LEN-1] = '\0';
            }

            // 解析端口号（0表示未设置）
            const char *port = uci_lookup_option_string(ctx, s, "port");
            cfg->port = port ? atoi(port) : 0;
        }
    }

cleanup:
    // 资源清理（逆序释放）
    if (pkg) uci_unload(ctx, pkg);  // 卸载配置包
    uci_free_context(ctx);          // 释放上下文
    return ret;
}

/**
 * IPv4地址格式验证辅助函数
 * @param ip 待验证的IP地址字符串
 * @return 1-有效 0-无效
 * 
 * 实现原理：
 * 1. 使用sscanf严格匹配四段数字格式
 * 2. 检查每个数字范围（0-255）
 * 3. 禁止出现多余字符（如192.168.1.1a）
 * 
 * 注意：不处理CIDR表示法（如/24）和IPv6地址
 */
static int is_valid_ipv4(const char *ip) {
    unsigned int octet[4];
    // 使用%*c检测多余字符（成功匹配会返回5，失败返回4）
    int count = sscanf(ip, "%u.%u.%u.%u%*c", 
                     &octet[0], &octet[1], &octet[2], &octet[3]);
    
    if (count != 4) return 0; // 段数不足或格式错误
    
    // 检查每个数字范围
    for (int i = 0; i < 4; i++) {
        if (octet[i] > 255) return 0;
    }
    return 1;
}

/**
 * 验证已加载的配置结构体
 * @param cfg 已加载的配置结构体
 * @return 错误码
 * 
 * 注意：不再从文件加载，直接验证内存中的配置
 */
int config_validate(const struct detector_config *cfg) {
    // 替换原有的UCI加载逻辑
    if (cfg->interval <= 0) {
        syslog(LOG_ERR, "无效的检测间隔: %d", cfg->interval);
        return ERR_INVALID_VALUE;
    }
    
    if (!is_valid_ipv4(cfg->ip)) {
        syslog(LOG_ERR, "无效的目标IP: %s", cfg->ip);
        return ERR_INVALID_VALUE;
    }
    // ... 其他验证逻辑 ...
} 