# 启动服务（安装后首次需要手动启动）
/etc/init.d/virtualgw start

# 设置开机自启
/etc/init.d/virtualgw enable

# 检查服务状态
/etc/init.d/virtualgw status
# 预期输出：virtualgw is running

# 重启服务（配置修改后）
/etc/init.d/virtualgw restart

# 停止服务
/etc/init.d/virtualgw stop

# 禁用开机自启
/etc/init.d/virtualgw disable

# 前台运行（调试模式）
/usr/bin/virtualgw -v -d

# 发送信号重载配置
killall -HUP virtualgw

# 强制终止进程
killall -9 virtualgw

# 检查进程树
pstree -p | grep virtualgw
# 预期输出：`-procd-+-virtualgw`

# 查看资源占用
top -p $(pgrep virtualgw)

# 检查文件描述符
ls -l /proc/$(pgrep virtualgw)/fd

# 实时查看日志
logread -f | grep virtualgw

# 查看历史日志
grep virtualgw /var/log/messages

# 设置详细日志级别（临时）
ubus call virtualgw command '{ "action": "set_loglevel", "param": "3" }'

# 检查UBus接口
ubus list | grep virtualgw

# 测试网络配置
ifstatus virtual_gw

# 验证防火墙规则
iptables -L -v | grep VGW_

ls -l /etc/init.d/virtualgw
# 应显示可执行权限：-rwxr-xr-x
