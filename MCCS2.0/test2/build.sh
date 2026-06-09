#!/bin/bash
set -e
g++ -std=gnu++17 -O2 -Wall -Wextra -o client_bench client_bench.cpp -lpthread
g++ -std=gnu++17 -O2 -Wall -Wextra -o client_qps  client_qps.cpp  -lpthread
echo "[*] client_bench + client_qps 编译完成"