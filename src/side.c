#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "config.h"
#include "network.h"
#include <stdlib.h>

/**
 * 检测外网连通性 - 改进版
 * @param detect_host 外网检测目标
 * @return 0=可达，1=不可达
 */
int detect_wan_connectivity(const char *detect_host) {
    char cmd[128];
    int success_count = 0;
    int total_attempts = 3;  // 增加检测次数
    
    for (int i = 0; i < total_attempts; i++) {
        // 构建ping命令，指定超时和目标地址
        snprintf(cmd, sizeof(cmd), "ping -c 1 -W 2 %s >/dev/null 2>&1", detect_host);
        
        // 执行命令并返回结果
        int ret = system(cmd);
        if (ret == 0) {
            success_count++;
        }
        
        // 短暂等待避免连续ping造成拥塞
        usleep(500000);  // 0.5秒
    }
    
    // 采用多数原则 - 至少有2次成功则认为连通
    return (success_count >= 2) ? 0 : 1;
}

void* side_loop(struct config *cfg) {
    syslog(LOG_INFO, "[Side] 旁路由服务已启动");
    while(1) {
        syslog(LOG_INFO, "[Side] 网络监测...");
        /* 外网检测逻辑 */
        int wan_status = detect_wan_connectivity(cfg->global.detect_src_addr);
        if (wan_status == 0) {
            syslog(LOG_INFO, "[Side] 外网通畅");
            enable_network_interface("virtual_gw");
            enable_ping_response();
        } else {
            syslog(LOG_INFO, "[Side] 外网不通");
            disable_network_interface("virtual_gw");
            disable_ping_response();
        }
        sleep(cfg->global.check_interval);
    }
    return NULL;
}
