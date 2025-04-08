#ifndef CONFIG_H
#define CONFIG_H

// UCI（统一配置接口）库头文件，用于解析配置文件
#include <uci.h>
// 系统日志库头文件，用于记录运行日志
#include <syslog.h>

// 网络地址最大长度（IPv4地址+端口）
#define MAX_IP_LEN 16
// 协议类型字符串最大长度（如"tcp"/"udp"/"icmp"）
#define MAX_PROTO_LEN 8

/**
 * 探测器配置结构体
 * 对应/etc/config/virtualgw配置文件中的参数
 */
struct detector_config {
    char ip[MAX_IP_LEN];        // 目标检测IP地址（对应配置中的ipaddr选项）
    int port;                   // 检测端口（TCP/UDP检测时生效）
    char proto[MAX_PROTO_LEN];  // 检测协议类型（icmp/tcp/udp）
    int interval;               // 检测间隔时间（秒）
    int retry_count;            // 失败重试次数阈值
    int log_level;              // 日志级别 0-关闭 1-基础 2-详细
};

/**
 * 加载配置文件到结构体
 * @param cfg 配置结构体指针
 * @return 错误码（见下方错误码定义）
 */
int config_load(struct detector_config *cfg);

/**
 * 验证配置文件有效性
 * 检查必需字段、值范围等
 * @return 错误码（见下方错误码定义）
 */
int config_validate();

/**
 * 错误码枚举定义
 * 负数表示错误，0表示成功
 */
enum {
    CONFIG_OK = 0,              // 配置操作成功
    ERR_UCI_LOAD = -1,          // UCI配置加载失败
    ERR_MISSING_SECTION = -2,   // 缺少必需的配置段（如global段）
    ERR_INVALID_VALUE = -3      // 无效的配置值（如非数字interval）
};

#endif 