#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uci.h>
#include <libubox/uloop.h>
#include <libubox/ustream.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/file.h>
#include "config.h" // 配置管理头文件
#include <string.h> // 添加此行以包含 strcmp 函数的声明

#define CONFIG_FILE "/etc/config/virtualgw" // 主配置文件路径

// UCI配置操作指针（用于读写配置）
struct uci_ptr ptr = {
    .package = "virtualgw",  // 配置包名称
    .section = "global"      // 默认操作配置段
};

// 在相关头文件中添加函数声明
int configure_network_interface(const struct detector_config *cfg);
int check_device_status(struct detector_config *cfg);
int enable_virtual_interface(void);
int disable_virtual_interface(void);

/**
 * 初始化网络接口配置
 * @return 状态码（0=成功，负数=错误码）
 * 
 * 执行流程：
 * 1. 加载网络接口配置参数
 * 2. 调用configure_network_interface创建/更新接口
 * 3. 如果失败则记录错误日志
 */
static int init_network_interface(const struct detector_config *cfg) {
    int ret = configure_network_interface(cfg); // 直接传递已加载的配置
    if (ret != 0) {
        syslog(LOG_ERR, "网络接口配置失败，错误码: %d", ret);
        return -1;
    }
    return 0;
}

/**
 * 主程序入口
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 程序退出状态码
 */
int main(int argc, char *argv[]) {
    struct detector_config cfg = {0}; // 配置结构体初始化
    
    // 1. 加载配置
    int config_status = config_load(&cfg);
    if (config_status != CONFIG_OK) {
        syslog(LOG_CRIT, "配置加载错误: %d", config_status);
        exit(EXIT_FAILURE);
    }

    // 2. 配置验证（新增前置验证）
    if (config_validate(&cfg) != CONFIG_OK) {
        syslog(LOG_CRIT, "配置验证失败");
        exit(EXIT_FAILURE); // 验证不通过直接退出
    }

    // 3. 初始化网络接口（使用已验证的配置）
    int network_init = init_network_interface(&cfg);
    if (network_init != 0) {
        exit(EXIT_FAILURE);
    }

    // 状态跟踪变量（-1=初始状态，0=正常模式，1=虚拟网关模式）
    static int current_gw_state = -1; 
    
    // 创建并锁定进程锁文件（防止多实例运行）
    int lock_fd = open("/var/lock/gw_switch.lock", O_CREAT|O_RDWR, 0666);

    // 主检测循环
    while(1) {
        flock(lock_fd, LOCK_EX); // 获取排他锁
        
        // 检测目标设备状态（0=离线，1=在线）
        int current_status = check_device_status(&cfg);
        
        /* 状态切换逻辑：
         * - 设备离线且未启用虚拟网关时：启用
         * - 设备恢复在线且虚拟网关仍启用时：禁用
         */
        if (current_status == 0 && current_gw_state != 1) { 
            syslog(LOG_WARNING, "设备离线，启用虚拟网关");
            if (enable_virtual_interface() == 0) {
                current_gw_state = 1; // 更新状态标记
            }
        } 
        else if (current_status == 1 && current_gw_state != 0) {
            syslog(LOG_NOTICE, "设备恢复，禁用虚拟网关");
            if (disable_virtual_interface() == 0) {
                current_gw_state = 0; // 重置状态标记
            }
        }
        
        flock(lock_fd, LOCK_UN); // 释放锁
        sleep(cfg.interval);     // 按配置间隔休眠
    }
    
    close(lock_fd);
    return 0;
}

/**
 * 检测目标设备可达性
 * @param cfg 包含检测参数的配置结构体
 * @return 1=设备在线 0=设备离线
 */
int check_device_status(struct detector_config *cfg) {
    char cmd[128]; // 命令缓冲区
    
    // 根据协议类型生成检测命令
    if (strcmp(cfg->proto, "tcp") == 0) {
        // TCP检测（使用netcat检查端口连通性）
        snprintf(cmd, sizeof(cmd), 
            "nc -z -w 2 %s %d > /dev/null", 
            cfg->ip, cfg->port);
    } else {
        // ICMP检测（使用ping命令）
        snprintf(cmd, sizeof(cmd), 
            "ping -c 1 -W 2 %s > /dev/null", 
            cfg->ip);
    }
    
    // 执行检测命令并获取返回值
    int ret = system(cmd);
    return WEXITSTATUS(ret) == 0 ? 1 : 0;
}

