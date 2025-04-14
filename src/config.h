#ifndef CONFIG_H
#define CONFIG_H

// UCI（统一配置接口）库头文件，用于解析配置文件
#include <uci.h>
// 系统日志库头文件，用于记录运行日志
#include <syslog.h>

// 错误码枚举定义
typedef enum {
    CONFIG_ERR_OK = 0,            // 成功
    CONFIG_ERR_MEM,               // 内存分配失败
    CONFIG_ERR_FILE,              // 文件操作错误
    CONFIG_ERR_SECTION,           // 配置段缺失
    CONFIG_ERR_INVALID_VALUE,     // 无效参数值
    CONFIG_ERR_MISSING_SECTION,   // 配置段缺失
    CONFIG_ERR_UCI_LOAD           // UCI配置加载失败
} config_error_t;

// 默认检测间隔（秒）
#define DEFAULT_INTERVAL 5

// 网络地址/域名最大长度
#define MAX_IP_LEN 256  // 增加到256以支持长域名（DNS标准允许最长253字符）
// 协议类型字符串最大长度（如"tcp"/"udp"/"icmp"）
#define MAX_PROTO_LEN 16
#define MAX_DEVICE_LEN 16
#define MAX_NAME_LEN 64

/**
 * 完整配置结构体
 * 对应/etc/config/virtualgw配置文件结构
 */
struct config {
    /*----- 全局配置 -----*/
    struct {
        int log_level;      // 日志级别 0-关闭 1-基础 2-详细
        char state[16];     // 路由状态 master/side
        char detect_src_addr[MAX_IP_LEN]; // 检测目标（可以是域名如www.baidu.com或IP地址）
        int check_interval; // 检测间隔（秒）
    } global;

    /*----- 虚拟接口配置 -----*/
    struct {
        char proto[MAX_PROTO_LEN];  // 协议类型 static/dhcp
        char device[MAX_DEVICE_LEN];// 绑定物理设备
        char ipaddr[MAX_IP_LEN];    // 虚拟接口IP
        char netmask[MAX_IP_LEN];   // 子网掩码
    } interface;
};

// 函数声明
config_error_t config_load(struct config *cfg);
extern int uci_get_int_default(struct uci_context *ctx, struct uci_section *s,
                              const char *option, int def);
extern int uci_get_bool_default(struct uci_context *ctx, struct uci_section *s,
                               const char *option, int def);

#endif 