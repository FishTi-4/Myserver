#!/bin/bash
# wrk压测脚本 - MCCS3.0
set -e

SERVER_DIR="/run/media/fishti/新加卷/Myserver/MCCS3.0"
SERVER_BIN="$SERVER_DIR/MCCS"
WRK_BIN="wrk"
HOST="http://localhost:8080/"
LOG_FILE="/tmp/mccs_wrk_test.log"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

cleanup() {
    echo -e "\n${YELLOW}>>> 清理中...${NC}"
    # 只杀 MCCS 二进制进程, 避免匹配脚本自身路径中的 "MCCS"
    pkill -9 -x "MCCS" 2>/dev/null || true
}

trap cleanup EXIT

# 1. 构建
echo -e "${GREEN}=== [1/4] 编译服务器 ===${NC}"
cd "$SERVER_DIR"
bash build.sh
echo "编译完成"

# 2. 启动服务器
echo -e "\n${GREEN}=== [2/4] 启动服务器 ===${NC}"
cleanup
sleep 0.3
nohup "$SERVER_BIN" > /dev/null 2>&1 &
sleep 1

if ! ss -tlnp | grep -q 8080; then
    echo -e "${RED}服务器启动失败${NC}"
    exit 1
fi
echo -e "服务器已启动 (PID: $(pgrep -f MCCS))"

# 3. 冒烟测试
echo -e "\n${GREEN}=== [3/4] 冒烟测试 ===${NC}"
if curl -s -o /dev/null -w "%{http_code}" "$HOST" | grep -q 200; then
    echo -e "HTTP 200 OK ✓"
else
    echo -e "${RED}HTTP 响应异常${NC}"
    exit 1
fi

# 4. wrk 压测
echo -e "\n${GREEN}=== [4/4] wrk 压测 ===${NC}"

run_test() {
    local conn="$1"
    local dur="$2"
    local label="$3"

    echo -e "\n${YELLOW}--- $label (${conn}连接, ${dur}s) ---${NC}"
    $WRK_BIN -c "$conn" -t 16 -d "$dur" "$HOST" 2>&1
}

run_test 1000 10 "▎ 1K"
run_test 10000 10 "▎ 10K"
run_test 100000 10 "▎ 100K"

echo -e "\n${GREEN}=== 压测完成 ===${NC}"
