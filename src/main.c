/**
 * @file main.c
 * @brief 虚拟网关主程序
 * 
 * 主要功能：
 * 1. 加载并验证配置文件
 * 2. 初始化网络接口配置
 * 3. 持续监测目标设备状态
 * 4. 根据设备状态切换虚拟网关
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>          // POSIX操作系统API（sleep等）
#include <uci.h>             // OpenWrt配置管理库
#include <libubox/uloop.h>   // 事件循环库
#include <libubox/ustream.h> // 流处理库
#include <getopt.h>          // 命令行参数解析
#include <fcntl.h>           // 文件控制选项
#include <sys/file.h>        // 文件锁定功能
#include <string.h>          // 字符串处理函数
#include <libubox/blobmsg.h> // UBus消息处理
#include "config.h"          // 配置文件解析头文件
#include "network.h"         // 网络接口管理头文件
#include <pthread.h>         // 线程库
#include "master.h"
#include "side.h"
#include <signal.h>
#include <errno.h>

// 常量定义
#define CONFIG_FILE "/etc/config/virtualgw" // 主配置文件路径
#define CONFIG_SUCCESS 0                    // 配置操作成功状态码
#define __COMMAND_ARGS_MAX 2                // 最大命令参数数量
#define EXIT_CONFIG_ERROR 10
#define EXIT_NETWORK_ERROR 20

// 锁文件定义
#define LOCK_MASTER "/var/lock/gw_master.lock"
#define LOCK_SIDE   "/var/lock/gw_side.lock"

// 全局锁描述符
static int master_lock_fd = -1;
static int side_lock_fd = -1;

// 预清理函数
static void cleanup_locks() {
    // 尝试清理主路由锁
    int pre_fd = open(LOCK_MASTER, O_CREAT|O_RDWR, 0666);
    if (pre_fd >= 0 && flock(pre_fd, LOCK_EX | LOCK_NB) == 0) {
        unlink(LOCK_MASTER);
        close(pre_fd);
    }
    
    // 尝试清理旁路由锁
    pre_fd = open(LOCK_SIDE, O_CREAT|O_RDWR, 0666);
    if (pre_fd >= 0 && flock(pre_fd, LOCK_EX | LOCK_NB) == 0) {
        unlink(LOCK_SIDE);
        close(pre_fd);
    }
}

// 信号处理
static void sig_handler(int sig) {
    syslog(LOG_NOTICE, "[Main] 收到终止信号 %d", sig);
    
    // 释放主路由锁
    if (master_lock_fd != -1) {
        flock(master_lock_fd, LOCK_UN);
        close(master_lock_fd);
        unlink(LOCK_MASTER);
    }
    
    // 释放旁路由锁
    if (side_lock_fd != -1) {
        flock(side_lock_fd, LOCK_UN);
        close(side_lock_fd);
        unlink(LOCK_SIDE);
    }
    
    exit(EXIT_SUCCESS);
}

/**
 * @struct uci_ptr
 * @brief UCI配置操作指针
 * 
 * 用于读写UCI配置的结构体，包含：
 * @var package 配置包名称（对应/etc/config/目录下的文件名）
 * @var section 配置段名称（配置文件中的section）
 */
struct uci_ptr ptr = {
    .package = "virtualgw",  // 操作virtualgw配置文件
    .section = "global"      // 默认操作global配置段
};


/**
 * @var command_policy
 * @brief UBus命令解析策略
 * 
 * 定义从UBus接收的命令参数结构：
 * [0] action - 要执行的操作（字符串类型）
 * [1] param  - 操作参数（字符串类型）
 */
const struct blobmsg_policy command_policy[__COMMAND_ARGS_MAX] = {
    [0] = { .name = "action", .type = BLOBMSG_TYPE_STRING }, // 操作类型
    [1] = { .name = "param",  .type = BLOBMSG_TYPE_STRING }  // 操作参数
};

/*-----------------------------------------------------------------------------
 * 主程序
 *----------------------------------------------------------------------------*/

/**
 * @brief 程序主入口
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 程序退出状态码
 * 
 * 主要执行流程：
 * 1. 加载并验证配置
 * 2. 初始化网络接口
 * 3. 初始化VRRP检测
 * 4. 进入主检测循环
 */
int main(int argc, char *argv[]) {
    openlog("virtualgw", LOG_PID|LOG_CONS, LOG_DAEMON);
    struct config cfg = {0}; // 初始化配置结构体
    
    //------------------------ 配置加载阶段 ------------------------
    syslog(LOG_ERR, "[main] 开始加载配置");
    int config_status = config_load(&cfg);
    if (config_status != CONFIG_ERR_OK) {
        syslog(LOG_CRIT, "[CONFIG] Load failed, error code: %d", config_status);
        exit(EXIT_CONFIG_ERROR);
    }
    syslog(LOG_ERR, "[main] 配置加载成功");

    //--------------------- 网络接口初始化阶段 ---------------------
    syslog(LOG_ERR, "[main] 开始初始化网络接口");
    int network_init = configure_network_interface(&cfg);
    if (network_init != 0) {
        syslog(LOG_CRIT, "[NETWORK] 网络接口初始化失败, 错误码: %d", network_init);
        exit(EXIT_NETWORK_ERROR);
    }
    syslog(LOG_ERR, "[main] 网络接口初始化成功");
    
    // 注册信号处理
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGQUIT, sig_handler);
    
    // 预清理
    cleanup_locks();
    
    // 根据角色获取对应锁
    int is_master = (strcmp(cfg.global.state, "master") == 0);
    const char *lock_file = is_master ? LOCK_MASTER : LOCK_SIDE;
    int *lock_fd = is_master ? &master_lock_fd : &side_lock_fd;
    
    // 获取角色专属锁
    *lock_fd = open(lock_file, O_CREAT|O_RDWR, 0666);
    if (flock(*lock_fd, LOCK_EX | LOCK_NB) != 0) {
        syslog(LOG_CRIT, "[Main] %s 已锁定，服务可能已运行", lock_file);
        exit(EXIT_FAILURE);
    }

    // 修改关闭逻辑
    if (is_master) { 
        syslog(LOG_ERR, "[main] 当前设备为主路由");
        master_loop(&cfg);
    }
    else {
        syslog(LOG_ERR, "[main] 当前设备为旁路由");
        side_loop(&cfg);
    }
    while(1) {
        pause();
    }
    closelog();
    return 0;
}