#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <syslog.h>
#include "status.h"

// 虚拟网关状态,全局变量，用于保存虚拟网关状态，-1=初始化，0=启用，1=禁用
int gw_status = -1;
/**
 * 检查网络接口是否启用
 * @param ifname 接口名称
 * @return 0=接口已启用并可用, 1=接口未启用或不可用, -1=查询失败
 */
int is_gw_up(const char *ifname) {
    char cmd[128];
    char buf[4096] = {0};
    FILE *fp;
    bool is_up = false;
    bool is_available = false;
    bool is_pending = true;
    
    // 构建ifstatus命令
    snprintf(cmd, sizeof(cmd), "ifstatus %s", ifname);
    
    // 执行命令并读取输出
    fp = popen(cmd, "r");
    if (!fp) {
        syslog(LOG_ERR, "[Network] 执行ifstatus命令失败");
        return -1;
    }
    
    // 读取命令输出
    if (fread(buf, 1, sizeof(buf) - 1, fp) <= 0) {
        syslog(LOG_ERR, "[Network] 读取ifstatus输出失败");
        pclose(fp);
        return -1;
    }
    pclose(fp);
    
    // 解析JSON结果中的关键字段
    if (strstr(buf, "\"up\": true")) {
        is_up = true;
    }
    
    if (strstr(buf, "\"available\": true")) {
        is_available = true;
    }
    
    if (strstr(buf, "\"pending\": false")) {
        is_pending = false;
    }
    
    // 接口已启用、可用且不在等待状态
    if (is_up && is_available && !is_pending) {
        return 0;
    }
    
    return 1;
}
