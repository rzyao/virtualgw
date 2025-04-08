#include <signal.h>
#include <syslog.h>

static void signal_handler(int sig) {
    switch(sig) {
    case SIGHUP:
        syslog(LOG_NOTICE, "Received SIGHUP, reloading config");
        reload_configuration();
        break;
    case SIGUSR1:
        syslog(LOG_NOTICE, "Received SIGUSR1, entering debug mode");
        set_debug_mode(true);
        break;
    }
}

void init_signals() {
    struct sigaction sa = {
        .sa_handler = signal_handler,
        .sa_flags = SA_RESTART
    };
    
    sigemptyset(&sa.sa_mask);
    sigaction(SIGHUP, &sa, NULL);  // 配置重载
    sigaction(SIGUSR1, &sa, NULL); // 调试模式
    sigaction(SIGTERM, &sa, NULL); // 优雅退出
} 