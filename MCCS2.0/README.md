# MCCS — Multi-Client Concurrent Server

基于 **epoll 边缘触发 (ET)** 的轻量级非阻塞 TCP 并发服务器。支持百万级客户端连接，内建 128 线程池将计算密集型任务异步化。纯 C++17 + POSIX 系统调用，零外部依赖。

## 快速开始

```bash
# 编译
bash build.sh

# 启动服务器
./MCCS 8080
```

在另一个终端发送消息：

```bash
echo "hello" | nc 127.0.0.1 8080
```

## 项目结构

```
MCCS2.0/
├── build.sh                  # 编译脚本
├── include/
│   ├── MCCS.h                # 全局头文件 + BUFFER_SIZE(32)
│   ├── CTCPserver.h          # 服务器 socket 类声明
│   ├── CLIENT.h              # 单连接状态类声明
│   └── ThreadPool.h          # 线程池 (header-only)
├── src/
│   ├── main.cpp              # 入口 — epoll 事件循环
│   ├── CTCPserver.cpp        # socket / bind / listen
│   └── client.cpp            # 连接状态 + 移动语义
└── test2/
    ├── client_bench.cpp       # 并发连接压测客户端
    ├── client_qps.cpp         # QPS 压测客户端
    ├── build.sh               # 压测编译
    ├── setup_bench.sh         # 系统调优 (需 root)
    ├── run_qps.sh             # 一键 QPS 压测 (1k→10k→100k)
    └── run_bench.sh           # 百万连接压测脚本
```

## 技术栈

| 项 | 说明 |
|---|------|
| 语言 | C++17 (gnu++17) |
| I/O 模型 | 非阻塞 + epoll 边缘触发 (EPOLLET) |
| 并发 | 单线程事件循环 + 128 工作线程池 |
| 内存管理 | `std::unique_ptr` + `std::make_unique` |
| 编译 | `g++ -std=gnu++17 -O2 -Iinclude src/*.cpp -o MCCS -lpthread` |
| 平台 | Linux ≥ 3.2.0 |

## 配置常量

| 常量 | 值 | 位置 | 说明 |
|------|-----|------|------|
| `BUFFER_SIZE` | 32 | `include/MCCS.h:33` | 每连接读缓冲区字节数 |
| `EPOLL_MAX` | 2²⁰ (1,048,576) | `src/main.cpp:8` | 最大 epoll 事件数 |
| 线程池 | 128 | `src/main.cpp:46` | 工作线程数 |
| listen backlog | 65535 | `src/CTCPserver.cpp:21` | accept 队列深度 |
| `SO_REUSEADDR` | 1 | `src/CTCPserver.cpp:13` | 允许快速重启 |

## 协议

服务器收到 32 字节以内的数据后原样回声。不识别消息边界，每次缓冲区填满后作为一条消息处理：

| 客户端消息 | 服务器行为 |
|-----------|-----------|
| 不以 `/` 开头 | 原样回声 |
| 以 `/` 开头 | 提交 `calc()` 到线程池（5 亿次求和），不回复 |

## 架构

```
epoll_wait (阻塞) → 遍历就绪事件:
  │
  ├─ 服务器 fd (EPOLLIN)
  │    while(1): accept4(SOCK_NONBLOCK) → client_list[fd] → ADD EPOLLIN|EPOLLOUT
  │
  ├─ EPOLLOUT
  │    while(1): send → 完成 → 切回读模式 / EAGAIN → 等待下次 EPOLLOUT
  │
  └─ EPOLLIN
       while(1): recv → 缓冲区满/EAGAIN/FIN
       → '/' 命令 → pool.post(calc) / 其他 → 原样回声
```

### Accept（边缘触发）

ET 模式下一次 EPOLLIN 可能对应多个积压连接。循环调用 `accept4(..., SOCK_NONBLOCK)` 直到返回 `EAGAIN`。每个新连接注册 `EPOLLIN | EPOLLOUT | EPOLLET`，`ev.data.fd` 为新 fd。

### Recv

循环 `recv` 直到以下任一条件触发退出：

1. **EAGAIN** — 内核缓冲区已空，处理已缓冲数据
2. **缓冲区满 (32B)** — 主动退出，避免 `recv(fd, buf, 0)` 误判 FIN
3. **bytes_read == 0** — 客户端关闭写端 (FIN)，断开连接

大于 32 字节的消息会被分片，每片单独回声。

### Send

非阻塞循环 `send`：
- 部分发送 → `outmessage.erase()` 逐步缩短
- 发完 → 切回读模式
- `EAGAIN` → 内核发送缓冲区满，等待 ET 触发下一轮 EPOLLOUT

## 压测

### 系统调优（首次，需 root）

```bash
sudo bash test2/setup_bench.sh
```

此脚本会：
- 设置 `ulimit -n 2000000`
- 扩展临时端口范围至 `1024-65535`
- 启用 `tcp_tw_reuse`
- 增大 `somaxconn` 至 65535
- 创建 loopback 别名 IP `127.0.0.2` ~ `127.0.0.20`

### 编译压测客户端

```bash
cd test2 && bash build.sh
```

### QPS 压测（一键脚本）

```bash
bash test2/run_qps.sh [端口] [源IP数] [测试秒]

# 默认参数 (8080 端口, 80 源IP, 5秒)
bash test2/run_qps.sh
```

实测数据 (80 源 IP，5 秒测试)：

| 连接数 | 建连 | 建连速率 | QPS |
|--------|------|----------|-----|
| 1,000 | 100% | 62,500/s | 452,856 |
| 10,000 | 100% | 166,667/s | 332,307 |
| 100,000 | 100% | 158,479/s | 229,665 |

### 手动 QPS 测试

```bash
./test2/client_qps 127.0.0.1 8080 <连接数> <测试秒> <源IP数>

# 例：10 万连接，5 秒测试，80 个源 IP
./test2/client_qps 127.0.0.1 8080 100000 5 80
```

`client_qps` 分两阶段：
1. **建立连接** — 非阻塞批量建连，实时显示成功/失败/速率
2. **收发测试** — 每条连接循环收发 30 字节回声消息，统计 QPS

### 并发连接保持测试

```bash
./test2/client_bench <host> <port> <连接数> [保持秒] [源IP数] [--verify]

# 例：10 万连接，保持 10 秒，带回声验证
./test2/client_bench 127.0.0.1 8080 100000 10 80 --verify
```

## 内存模型

```
启动时预分配:
  client_list  = unique_ptr<client[]>(1,048,576)    ~24 MB
  events       = unique_ptr<epoll_event[]>(1,048,576) ~16 MB
  ThreadPool   = 128 线程                             ~1 MB
  ─────────────────────────────────────────────────────────
  总计                                                ~41 MB
```

每个新连接复用已分配的 `client` 槽位，无额外堆分配。

## 构建

```bash
bash build.sh
```