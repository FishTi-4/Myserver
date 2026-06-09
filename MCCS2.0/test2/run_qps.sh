#!/bin/bash
# MCCS QPS 压测脚本 — 自动启动服务器、依次测试 1k/10k/100k、停止服务器
# 用法: bash run_qps.sh [端口] [源IP数] [测试秒]
# 默认: port=8080  ips=80  secs=5

set -e

PORT="${1:-8080}"
IPS="${2:-80}"
SECS="${3:-5}"
SERVER_DIR="$(dirname "$0")/.."
TEST_DIR="$(dirname "$0")"

echo "============================================"
echo "  MCCS QPS 压测"
echo "============================================"
echo "  端口:     $PORT"
echo "  源IP数:   $IPS"
echo "  测试时长: ${SECS}s"
echo "============================================"

# 编译
echo ""
echo "[1/5] 编译服务器..."
(cd "$SERVER_DIR" && bash build.sh)
echo "[2/5] 编译客户端..."
(cd "$TEST_DIR" && bash build.sh)

# 清理旧进程
echo ""
echo "[3/5] 清理旧进程..."
pkill -f "MCCS $PORT" 2>/dev/null || true
sleep 1

# 启动服务器
echo "[4/5] 启动服务器..."
nohup "$SERVER_DIR/MCCS" "$PORT" > /tmp/mccs_qps.log 2>&1 &
disown
sleep 1

if ! pgrep -f "MCCS $PORT" > /dev/null; then
    echo "ERROR: 服务器启动失败"
    cat /tmp/mccs_qps.log
    exit 1
fi
echo "  服务器 PID: $(pgrep -f "MCCS $PORT")"

# 压测
echo ""
echo "[5/5] 开始压测..."
echo ""

CLIENT="$TEST_DIR/client_qps"

echo ">>> 1k 连接测试 <<<"
"$CLIENT" 127.0.0.1 "$PORT" 1000 "$SECS" "$IPS"
echo ""

echo ">>> 10k 连接测试 <<<"
"$CLIENT" 127.0.0.1 "$PORT" 10000 "$SECS" "$IPS"
echo ""

echo ">>> 100k 连接测试 <<<"
"$CLIENT" 127.0.0.1 "$PORT" 100000 "$SECS" "$IPS"
echo ""

# 停止
echo "============================================"
echo "  停止服务器..."
kill $(pgrep -f "MCCS $PORT") 2>/dev/null || true
echo "  测试完成"
