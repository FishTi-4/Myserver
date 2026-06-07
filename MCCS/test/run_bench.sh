#!/bin/bash
# MCCS 百万级压测执行脚本
# 先运行: sudo bash setup_bench.sh

set -e

SERVER_DIR="$(dirname "$0")/.."
TEST_DIR="$(dirname "$0")"
PORT=8080
CONNECTIONS="${1:-1000000}"
KEEPALIVE="${2:-10}"
SOURCE_IPS="${3:-20}"

echo "============================================"
echo "  MCCS 百万级并发连接压测"
echo "============================================"
echo "  目标连接数: $CONNECTIONS"
echo "  保持时间:   ${KEEPALIVE}s"
echo "  源 IP 数量: $SOURCE_IPS"
echo "  理论上限:   $((SOURCE_IPS * 60000))"
echo "============================================"

# 检查 fd 限制
FD_LIMIT=$(ulimit -n)
echo ""
echo "[检查] fd limit: $FD_LIMIT"
if [ "$FD_LIMIT" -lt "$CONNECTIONS" ]; then
    echo "  ERROR: fd limit ($FD_LIMIT) < 目标连接数 ($CONNECTIONS)"
    echo "  请先执行: sudo bash $TEST_DIR/setup_bench.sh"
    exit 1
fi

# 检查 loopback 别名
LO_IPS=$(ip addr show lo | grep "inet 127" | wc -l)
echo "[检查] loopback IPs: $LO_IPS"
if [ "$LO_IPS" -lt "$SOURCE_IPS" ]; then
    echo "  ERROR: 需要 $SOURCE_IPS 个 loopback IP，当前只有 $LO_IPS 个"
    echo "  请先执行: sudo bash $TEST_DIR/setup_bench.sh"
    exit 1
fi

# 检测临时端口范围
PORT_RANGE=$(cat /proc/sys/net/ipv4/ip_local_port_range)
PORTS_AVAIL=$(( $(echo $PORT_RANGE | awk '{print $2}') - $(echo $PORT_RANGE | awk '{print $1}') ))
echo "[检查] 可用临时端口数: $PORTS_AVAIL (每个源 IP)"

# 构建
echo ""
echo "[构建] 服务器..."
(cd "$SERVER_DIR" && bash build.sh)
echo "[构建] 压测客户端..."
(cd "$TEST_DIR" && bash build.sh)

# 启动服务器
echo ""
echo "[启动] 服务器..."
kill $(pgrep -f "./MCCS $PORT") 2>/dev/null || true
sleep 1
(cd "$SERVER_DIR" && ./MCCS $PORT &>/tmp/mccs_server.log) &
sleep 2

if ! pgrep -f "./MCCS $PORT" > /dev/null; then
    echo "  ERROR: 服务器启动失败"
    cat /tmp/mccs_server.log
    exit 1
fi
echo "  服务器 PID: $(pgrep -f "./MCCS $PORT")"

# 压测
echo ""
echo "============================================"
echo "  开始压测..."
echo "============================================"
(cd "$TEST_DIR" && ./client_bench 127.0.0.1 $PORT $CONNECTIONS $KEEPALIVE $SOURCE_IPS)

# 查看服务器统计
echo ""
echo "[服务器最近日志]"
tail -5 /tmp/mccs_server.log
echo ""
echo "[服务器状态]"
ps -o pid,rss,vsz,pcpu,pmem -p $(pgrep -f "./MCCS $PORT") 2>/dev/null
echo ""
echo "[连接数统计]"
ss -tn state established dst :$PORT 2>/dev/null | tail -1 || echo "无法统计"

# 清理
kill $(pgrep -f "./MCCS $PORT") 2>/dev/null || true
echo ""
echo "测试完成，服务器已停止"
