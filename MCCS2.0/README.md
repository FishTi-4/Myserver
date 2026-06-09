# MCCS — Multi-Client Concurrent Server

基于 **Linux epoll 边缘触发 (ET)** 的轻量级非阻塞 TCP 并发服务器。支持百万级客户端连接，内建 128 线程池将计算密集型任务异步化。纯 C++17 + POSIX 系统调用，零外部依赖。

---

## 快速开始

```bash
# 编译
./build.sh

# 启动（端口 8080）
./MCCS 8080
```

## 项目结构

```
MCCS/
├── build.sh              # 编译脚本
├── include/
│   ├── MCCS.h            # 全局头文件 + BUFFER_SIZE 宏
│   ├── CTCPserver.h      # 服务器 socket 类声明
│   ├── CLIENT.h          # 单连接状态类声明
│   └── ThreadPool.h      # 线程池 (header-only)
├── src/
│   ├── main.cpp          # 入口 — epoll 事件循环
│   ├── CTCPserver.cpp    # socket / bind / listen
│   └── client.cpp        # 连接状态 + 移动语义
└── test/
    ├── client_bench.cpp  # 并发压测客户端
    ├── build.sh          # 压测编译
    ├── run_bench.sh      # 一键压测脚本
    └── setup_bench.sh    # 系统调优 (需 root)
```

## 技术栈

| 项 | 说明 |
|---|------|
| 语言 | C++17 (gnu++17) |
| I/O 模型 | 非阻塞 + epoll 边缘触发 (EPOLLET) |
| 并发 | 单线程事件循环 + 独立线程池 |
| 内存管理 | std::unique_ptr + std::make_unique |
| 链接 | -lpthread |
| 平台 | Linux ≥ 3.2.0 |
| 外部依赖 | 无 |

## 配置常量

| 常量 | 值 | 位置 | 说明 |
|------|-----|------|------|
| `BUFFER_SIZE` | 32 | `include/MCCS.h:33` | 每连接读缓冲区字节数 |
| `EPOLL_MAX` | 2²⁰ (1,048,576) | `src/main.cpp:8` | 最大 epoll 事件数/最大客户端数 |
| 线程池线程数 | 128 | `src/main.cpp:45` | 工作线程数量 |
| listen backlog | 5 | `src/CTCPserver.cpp:21` | 连接等待队列深度 |

## 协议

| 客户端消息 | 服务器行为 |
|-----------|-----------|
| 不以 `/` 开头 | 逐片回声 `服务器已收到消息: <内容>` |
| 以 `/` 开头 | 提交 `calc()` 到线程池，不回复 |

## 架构

```
main.cpp (事件循环)
  ├── ctcpserver          — 非阻塞 TCP socket 创建/绑定/监听
  ├── epoll (ET 模式)      — 事件驱动 I/O，管理所有 fd
  ├── client[N]           — 每连接状态 (fd, 32B 读缓冲, 响应字符串)
  └── ThreadPool (128线程) — 处理 '/' 开头的计算任务
```

### 事件循环处理流程

```
epoll_wait (阻塞) → 遍历就绪事件:
  │
  ├─ 服务器 fd (accept)
  │    while(1): accept4(SOCK_NONBLOCK) → client_list[fd] → ADD EPOLLIN
  │
  ├─ EPOLLOUT (发送响应)
  │    while(1): send → 部分发送/完成 → MOD EPOLLIN
  │
  └─ EPOLLIN (接收消息)
       while(1): recv → 缓冲区满/EAGAIN/FIN → break
       → 处理缓冲数据 → '/'任务 / 回声响应
```

### 关键行为

**accept**

ET 模式下一次 EPOLLIN 可能对应多个积压连接。循环调用 `accept4(..., SOCK_NONBLOCK)` 直到返回 `EAGAIN`，一次性接纳所有待 accept 的连接。每个新连接通过 `epoll_ctl ADD` 注册 EPOLLIN | EPOLLET，`ev.data.fd` 设为新 fd 用于后续事件分发。

**recv（接收消息）**

循环 `recv` 直到三种情况之一发生：
1. **EAGAIN** — 内核缓冲区已空，退出循环，处理已缓冲数据。
2. **缓冲区满** (32 字节) — 主动退出循环。若继续调用 `recv(fd, buf+32, 0, 0)` 传入 length=0，Linux 返回 0，与 TCP 有序关闭无法区分，导致误断连。
3. **bytes_read == 0** — 客户端关闭写端 (FIN)。
   - 若 `buffer_get_size > 0`：先退出循环处理已缓冲数据（生成回声），下次 EPOLLIN 再由 FIN 触发正常断连。
   - 若 `buffer_get_size == 0`：直接断连，从 epoll 删除 fd，关闭 socket。

服务器不识别消息边界。每次循环退出把缓冲数据作为一条消息处理。大于 32 字节的消息会被分片，每片单独回声。

**send（发送响应）**

非阻塞循环 `send`：
- 成功发送 → `outmessage.erase(0, bytes_sent)` 逐步缩短 → 发完则 `epoll_ctl MOD EPOLLIN` 切回读模式。
- `EAGAIN` → 内核发送缓冲区满，直接退出（不 MOD）。ET 模式下 fd 保持注册 EPOLLOUT，缓冲区释放时自动触发下一轮发送。
- 非 EAGAIN 错误 → 从 epoll 删除 fd，关闭 socket。

**线程池（`/` 命令）**

`pool.post(calc)` 将计算任务提交到 128 个工作线程。`calc()` 执行 5 亿次循环求和。提交后不发送响应，直接切回 EPOLLIN 继续等待消息。

## 压测

```bash
# 系统调优 (仅首次，需 root)
sudo ./test/setup_bench.sh

# 编译压测客户端
cd test && ./build.sh

# 10000 并发连接 + 回声验证
./client_bench 127.0.0.1 8080 10000 10 --verify

# 百万连接压测
./run_bench.sh
```

压测客户端 (`test/client_bench.cpp`) 支持批量建连（每批最多 10000）、原子计数、状态机驱动的回声称验证。`--verify` 模式在连接建立后发送 "PING"，等待服务器回声 `服务器已收到消息: PING`。

## 内存模型

```
启动时分配:
  client_list  = unique_ptr<client[]>(1048576)     ~24 MB (1M client 对象)
  events       = unique_ptr<epoll_event[]>(1048576) ~16 MB (1M epoll_event)
  ThreadPool   = 128 线程栈空间                      ~1 MB
  ─────────────────────────────────────────────────────────
  总计                                              ~41 MB

每个新连接:
  client_list[fd] = move-assign(client(fd))  ← 无额外堆分配（复用已分配 buffer）
  epoll 内核新增一个 fd 条目
```

## 构建

```bash
./build.sh
# 等效: g++ -std=gnu++17 -Iinclude src/*.cpp -o MCCS -lpthread
```

## 许可证

MIT
