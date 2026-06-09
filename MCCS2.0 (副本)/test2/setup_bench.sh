#!/bin/bash
# MCCS 百万级压测系统配置脚本
# 需要 sudo 运行: sudo bash setup_bench.sh

set -e

echo "=== MCCS 百万级压测系统调参 ==="

# 1. 文件描述符上限
echo "[1/6] 调整文件描述符上限..."
ulimit -n 2000000
echo "* soft nofile 2000000" >> /etc/security/limits.conf
echo "* hard nofile 2000000" >> /etc/security/limits.conf
echo "  fd limit set to 2000000"

# 2. 扩展临时端口范围
echo "[2/6] 扩展临时端口范围..."
echo "1024 65535" > /proc/sys/net/ipv4/ip_local_port_range
echo "  端口范围: $(cat /proc/sys/net/ipv4/ip_local_port_range)"

# 3. 启用 TIME_WAIT 复用
echo "[3/6] 启用 tcp_tw_reuse..."
echo 1 > /proc/sys/net/ipv4/tcp_tw_reuse
echo "  tcp_tw_reuse = 1"

# 4. 增大 SYN backlog
echo "[4/6] 增大 somaxconn..."
echo 65535 > /proc/sys/net/core/somaxconn
echo "  somaxconn = $(cat /proc/sys/net/core/somaxconn)"

# 5. 增大 TCP 内存 (可选，预防 OOM)
echo "[5/6] 调整 TCP 缓冲区..."
echo "4096 87380 6291456" > /proc/sys/net/ipv4/tcp_wmem
echo "4096 87380 6291456" > /proc/sys/net/ipv4/tcp_rmem

# 6. 创建 loopback 别名 IP（绕开 64K 端口限制）
echo "[6/6] 创建 loopback 别名 IP..."
for i in $(seq 2 20); do
    if ! ip addr show lo | grep -q "127.0.0.$i/8"; then
        ip addr add 127.0.0.$i/8 dev lo
        echo "  添加: 127.0.0.$i"
    fi
done

echo ""
echo "=== 配置完成 ==="
echo "请重新登录或执行: ulimit -n 2000000"
echo ""
echo "验证:"
echo "  ulimit -n        = $(ulimit -n)"
echo "  port range       = $(cat /proc/sys/net/ipv4/ip_local_port_range)"
echo "  somaxconn        = $(cat /proc/sys/net/core/somaxconn)"
echo "  loopback IPs     = $(ip addr show lo | grep 'inet 127' | wc -l)"
