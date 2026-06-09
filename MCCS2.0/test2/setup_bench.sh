#!/bin/bash
# MCCS 百万级压测系统配置脚本（增强版）
# 必须 root 运行，执行后某些参数需重启或重新登录生效

set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "请使用 root 执行: sudo bash $0"
    exit 1
fi

echo "=== MCCS 百万级压测系统调参 ==="

# ==========================================
# 1. 进程级文件描述符限制
# ==========================================
echo "[1/9] 设置进程文件描述符上限..."
# 当前 shell 立即生效
ulimit -n 2000000
# 持久化（注意：如果文件中已有则先删除旧字段，避免重复行）
sed -i '/^\*.*nofile/d' /etc/security/limits.conf
echo "* soft nofile 2000000" >> /etc/security/limits.conf
echo "* hard nofile 2000000" >> /etc/security/limits.conf
echo "  fd limit set to 2000000 (需重新登录或 su - 生效)"

# ==========================================
# 2. 系统全局文件描述符上限
# ==========================================
echo "[2/9] 调整内核级文件描述符上限..."
sysctl -w fs.file-max=2097152
sysctl -w fs.nr_open=2097152
echo "  fs.file-max = $(cat /proc/sys/fs/file-max)"

# ==========================================
# 3. 本地端口范围 & TIME_WAIT 优化
# ==========================================
echo "[3/9] 扩展临时端口范围..."
# 将动态端口范围扩展到 1024-65535
echo "1024 65535" > /proc/sys/net/ipv4/ip_local_port_range

echo "[4/9] 启用 TIME_WAIT 快速复用..."
# 开启重用（需 tcp_timestamps=1，默认开启）
sysctl -w net.ipv4.tcp_tw_reuse=1
# 减少 TIME_WAIT 时长（默认60秒改为10秒，加快回收）
sysctl -w net.ipv4.tcp_fin_timeout=10
# 注意：tw_recycle 已在 4.12 内核中移除，勿用

# ==========================================
# 4. 全/半连接队列深度
# ==========================================
echo "[5/9] 调大连接队列..."
sysctl -w net.core.somaxconn=65535            # 全连接队列（listen backlog 上限）
sysctl -w net.ipv4.tcp_max_syn_backlog=65535 # 半连接队列上限
sysctl -w net.core.netdev_max_backlog=65535  # 网卡接收队列

# ==========================================
# 5. TCP 缓冲区与内存
# ==========================================
echo "[6/9] 调整 TCP 缓冲区大小..."
# 读写内存最小值、默认值、最大值
sysctl -w net.ipv4.tcp_rmem="4096 87380 26214400"
sysctl -w net.ipv4.tcp_wmem="4096 65536 26214400"
# 全局 TCP 可用内存页数（调高以避免 OOM 丢包）
sysctl -w net.ipv4.tcp_mem="786432 1048576 1572864"

# ==========================================
# 6. 快速回收 & 重传优化
# ==========================================
echo "[7/9] TCP 快速开启与重传..."
# 开启 TCP Fast Open（客户端和服务端都需支持）
sysctl -w net.ipv4.tcp_fastopen=3
# 尽早重传，避免长时间等待
sysctl -w net.ipv4.tcp_early_retrans=1

# ==========================================
# 7. 防止 SYN flood 冲刷
# ==========================================
echo "[8/9] 关闭 SYN cookies（高并发压测建议关闭，避免性能损失）..."
sysctl -w net.ipv4.tcp_syncookies=0

# 注意：生产网络对抗攻击时建议开启，纯压测环境下关闭可提升吞吐

# ==========================================
# 8. 本地回环接口别名 IP
# ==========================================
echo "[9/9] 创建 loopback 别名 IP（多源IP突破6.5万连接）..."
for i in $(seq 2 40); do          # 增加到 40 个，支持更大规模测试
    if ! ip addr show lo | grep -q "127.0.0.$i/8"; then
        ip addr add 127.0.0.$i/8 dev lo
        echo "  添加: 127.0.0.$i"
    fi
done

echo ""
echo "=== 调优完成 ==="
echo "请重启或重新登录以确保所有参数生效。"
echo ""
echo "核心参数验证："
echo "  ulimit -n         = $(ulimit -n)"
echo "  fs.file-max       = $(cat /proc/sys/fs/file-max)"
echo "  port range        = $(cat /proc/sys/net/ipv4/ip_local_port_range)"
echo "  somaxconn         = $(cat /proc/sys/net/core/somaxconn)"
echo "  syn backlog       = $(cat /proc/sys/net/ipv4/tcp_max_syn_backlog)"
echo "  tw_reuse          = $(cat /proc/sys/net/ipv4/tcp_tw_reuse)"
echo "  fin_timeout       = $(cat /proc/sys/net/ipv4/tcp_fin_timeout)"
echo "  tcp_rmem          = $(cat /proc/sys/net/ipv4/tcp_rmem)"
echo "  tcp_wmem          = $(cat /proc/sys/net/ipv4/tcp_wmem)"
echo "  loopback IPs      = $(ip addr show lo | grep 'inet 127' | wc -l)"