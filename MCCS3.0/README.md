# MCCS3.0

基于 Linux **io_uring** 的单线程、非阻塞、高并发 TCP/HTTP Echo 服务器。

## 特性

- **io_uring 异步 I/O** — 使用 Linux 内核原生异步 I/O 接口，极致吞吐
- **单线程事件循环** — 无锁设计，无上下文切换开销
- **百万级并发** — 理论上支持最多 1,048,576 (1<<20) 个并发连接
- **自动协议检测** — 客户端发送 HTTP 请求时自动返回 `200 OK`，否则回显原始数据
- **小体积** — ~280 行 C++ 源码，无第三方依赖（除 liburing）

## 快速开始

### 依赖

| 依赖 | 安装命令（Debian/Ubuntu） |
|------|--------------------------|
| g++ (支持 C++23) | `sudo apt install g++` |
| liburing | `sudo apt install liburing-dev` |
| wrk (可选，压测用) | `sudo apt install wrk` |

### 编译[text](total.cpp)

```bash
bash build.sh
```

或手动编译：

```bash
g++ -std=gnu++23 -Iinclude src/*.cpp -o MCCS -luring
```

### 运行

```bash
./MCCS
```

服务器默认监听 `0.0.0.0:8080`。

### 测试

```bash
# 冒烟测试
curl http://localhost:8080/

# 完整压测流程（编译 + 启动 + 测试 + wrk 压测）
bash wrk_bench.sh
```

## 架构

```
┌──────────────┐
│  Main Loop   │
│  (io_uring)  │
└──────┬───────┘
       │  submit & reap CQEs
       ▼
┌─────────────────────────────┐
│  Event Dispatch             │
│  ACCEPT / READ / WRITE      │
└──────┬──────────────────────┘
       │
       ▼
┌──────────────┐    ┌──────────────┐
│  connection  │    │  connection  │  ...  (最多 1M 个)
│  fd=N        │    │  fd=N+1      │
└──────────────┘    └──────────────┘
```

### 事件类型

| 事件 | 说明 |
|------|------|
| `ACCEPT_EVENT` | 接受新连接 |
| `READ_EVENT` | 读取客户端数据 |
| `WRITE_EVENT` | 向客户端写回响应 |

### 协议处理

读取数据后，检测前 4 字节：

- 以 `GET ` / `POST` / `HEAD` 开头 → 返回 `HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: keep-alive\r\n\r\nhello`
- 其他 → 原样回显

## 项目结构

```
MCCS3.0/
├── build.sh          # 编译脚本
├── wrk_bench.sh      # 自动化压测脚本
├── include/
│   ├── MCCS.h        # 公共头文件与常量
│   ├── CTCPserver.h  # TCP 服务端套接字封装
│   └── connection.h  # 连接类、事件类型、缓冲区定义
└── src/
    ├── main.cpp      # 入口、io_uring 事件循环、HTTP 处理
    ├── CTCPserver.cpp
    └── connection.cpp
```

## 配置

所有配置项均为编译时常量：

| 常量 | 值 | 位置 | 说明 |
|------|----|------|------|
| 监听端口 | `8080` | `main.cpp:189` | `run_server("8080")` |
| IO 缓冲区 | `128` | `connection.h:12` | 读写缓冲区大小 |
| 队列深度 | `32768` | `main.cpp:9` | io_uring 提交队列深度 |
| 最大连接数 | `1048576` | `main.cpp:10` | `1 << 20` |
| 监听队列 | `65535` | `CTCPserver.cpp:25` | listen backlog |

## 压测数据

运行 `bash wrk_bench.sh` 可得到类似结果（测试环境：本地 loopback）：

- **1K 并发 / 10s** — 百万级 QPS
- **10K 并发 / 10s** — 高吞吐，低延迟

## License

MIT
