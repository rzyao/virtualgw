/**
 * @file master.c
 * @brief 主路由控制模块
 * 
 * 主要功能：
 * 1. 持续检测旁路由的连通性
 * 2. 获取旁路由的状态
 * 3. 根据检测结果控制虚拟网关状态
 */
#define _GNU_SOURCE
#include "config.h"
#include "network.h"
#include <pthread.h>
#include <sys/file.h>
#include <fcntl.h>   // 解决O_CREAT错误
#include <unistd.h>  // 解决close函数声明
#include <stdlib.h>    // for system()


int detect_lan_peer(const char *peer_ip) {
    char cmd[128];
    int success_count = 0;
    int total_attempts = 3;
    
    for (int i = 0; i < total_attempts; i++) {
        snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s >/dev/null 2>&1", peer_ip);
        int ret = system(cmd);
        if (ret == 0) {
            success_count++;
        }
        usleep(500000);  // 0.5秒
    }
    
    return (success_count >= 2) ? 0 : 1;
}

/**
 * @brief 主检测循环线程函数
 * @return void* 线程退出状态
 */
void* master_loop(struct config *cfg) {
    syslog(LOG_INFO, "[Master] 主路由服务已启动");
    while(1) {
        /* 旁路由连通性检测 */
        int peer_online = detect_lan_peer(cfg->global.detect_src_addr);
        
        // 仅当旁路由在线时查询其外网状态
        if (peer_online == 0) { 
            syslog(LOG_INFO, "[Master] 旁路由在线");
            disable_network_interface("virtual_gw");
        }
        else {
            syslog(LOG_INFO, "[Master] 旁路由离线");
            enable_network_interface("virtual_gw");
        }
        sleep(cfg->global.check_interval); // 等待下一个检测周期
    }
    return NULL;
}

