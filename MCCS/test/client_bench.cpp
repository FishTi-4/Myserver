#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <thread>

using namespace std;

const int MAX_EVENTS = 1024;
const int BUFFER_SIZE = 4096;

struct BenchConfig
{
    string host;
    int port;
    int total_connections;
    int keep_alive_sec;
    bool verify_echo;
};

struct Connection
{
    int fd = -1;
    enum State
    {
        CONNECTING,
        CONNECTED,
        SENDING,
        RECEIVING,
        DONE,
        FAILED
    } state = CONNECTING;
    string send_buf;
    string recv_buf;
    size_t send_offset = 0;
};

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void print_usage(const char *prog)
{
    cerr << "用法: " << prog << " <host> <port> <连接数> [保持时间秒] [--verify]\n"
         << "示例: " << prog << " 127.0.0.1 8080 10000 10 --verify\n"
         << "  --verify  连接后发送测试消息并验证回声响应\n";
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        print_usage(argv[0]);
        return 1;
    }

    BenchConfig cfg;
    cfg.host = argv[1];
    cfg.port = stoi(argv[2]);
    cfg.total_connections = stoi(argv[3]);
    cfg.keep_alive_sec = (argc >= 5) ? stoi(argv[4]) : 30;
    cfg.verify_echo = false;

    for (int i = 5; i < argc; i++)
    {
        if (string(argv[i]) == "--verify")
            cfg.verify_echo = true;
    }

    cout << "=== MCCS 并发连接压测 ===\n"
         << "目标: " << cfg.host << ":" << cfg.port << "\n"
         << "并发连接数: " << cfg.total_connections << "\n"
         << "保持时间: " << cfg.keep_alive_sec << " 秒\n"
         << "回声验证: " << (cfg.verify_echo ? "是" : "否") << "\n\n";

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(cfg.port);
    if (inet_pton(AF_INET, cfg.host.c_str(), &server_addr.sin_addr) <= 0)
    {
        cerr << "无效的主机地址: " << cfg.host << endl;
        return 1;
    }

    int epfd = epoll_create1(0);
    if (epfd == -1)
    {
        cerr << "创建 epoll 实例失败" << endl;
        return 1;
    }

    // 批量创建 socket 并开始连接
    vector<Connection> conns(cfg.total_connections);
    atomic<int> connected{0};
    atomic<int> failed{0};

    auto start_time = chrono::steady_clock::now();

    int batch_size = min(cfg.total_connections, 10000);
    for (int batch = 0; batch < cfg.total_connections; batch += batch_size)
    {
        int batch_end = min(batch + batch_size, cfg.total_connections);
        for (int i = batch; i < batch_end; i++)
        {
            int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (fd == -1)
            {
                conns[i].state = Connection::FAILED;
                failed++;
                continue;
            }
            conns[i].fd = fd;

            int ret = connect(fd, (sockaddr *)&server_addr, sizeof(server_addr));
            if (ret == -1 && errno != EINPROGRESS)
            {
                close(fd);
                conns[i].fd = -1;
                conns[i].state = Connection::FAILED;
                failed++;
                continue;
            }

            epoll_event ev{};
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.ptr = &conns[i];
            if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1)
            {
                close(fd);
                conns[i].fd = -1;
                conns[i].state = Connection::FAILED;
                failed++;
            }
        }

        // 处理这批连接的结果
        bool batch_done = false;
        while (!batch_done)
        {
            epoll_event events[MAX_EVENTS];
            int n = epoll_wait(epfd, events, MAX_EVENTS, 1000);
            if (n == 0)
                break;

            batch_done = true;
            for (int j = 0; j < n; j++)
            {
                Connection *c = (Connection *)events[j].data.ptr;
                if (c->state != Connection::CONNECTING)
                {
                    batch_done = false;
                    continue;
                }

                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0)
                {
                    c->state = Connection::CONNECTED;
                    connected++;

                    if (cfg.verify_echo)
                    {
                        c->send_buf = "PING";
                        c->state = Connection::SENDING;
                        epoll_event ev{};
                        ev.events = EPOLLOUT | EPOLLET;
                        ev.data.ptr = c;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                    }
                    else
                    {
                        // 只保活，不再监听事件，减少 epoll 压力
                        epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                    }
                }
                else
                {
                    close(c->fd);
                    c->fd = -1;
                    c->state = Connection::FAILED;
                    failed++;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                }
            }
        }

        cout << "\r创建连接: " << batch_end << "/" << cfg.total_connections
             << "  成功: " << connected << "  失败: " << failed << flush;
    }

    auto connect_end = chrono::steady_clock::now();
    auto connect_ms = chrono::duration_cast<chrono::milliseconds>(connect_end - start_time).count();

    cout << "\n\n--- 建连完成 ---\n"
         << "成功: " << connected << "\n"
         << "失败: " << failed << "\n"
         << "耗时: " << connect_ms << " ms\n"
         << "速率: " << (connected * 1000.0 / max(connect_ms, 1L)) << " 连接/秒\n\n";

    // 回声验证阶段
    if (cfg.verify_echo && connected > 0)
    {
        cout << "开始回声验证..." << endl;

        int echo_success = 0;
        int echo_failed = 0;
        auto verify_start = chrono::steady_clock::now();

        double verify_deadline = chrono::duration<double>(verify_start.time_since_epoch()).count() + 5.0;

        while (echo_success + echo_failed < connected)
        {
            epoll_event events[MAX_EVENTS];
            int n = epoll_wait(epfd, events, MAX_EVENTS, 500);
            if (n <= 0)
            {
                auto now = chrono::steady_clock::now();
                double sec = chrono::duration<double>(now.time_since_epoch()).count();
                if (sec > verify_deadline)
                {
                    // 超时，剩余未验证的算失败
                    failed = connected - echo_success;
                    break;
                }
                continue;
            }

            for (int j = 0; j < n; j++)
            {
                Connection *c = (Connection *)events[j].data.ptr;

                if (events[j].events & EPOLLOUT && c->state == Connection::SENDING)
                {
                    ssize_t sent = send(c->fd, c->send_buf.c_str() + c->send_offset,
                                        c->send_buf.size() - c->send_offset, 0);
                    if (sent > 0)
                    {
                        c->send_offset += sent;
                        if (c->send_offset >= c->send_buf.size())
                        {
                            c->state = Connection::RECEIVING;
                            epoll_event ev{};
                            ev.events = EPOLLIN | EPOLLET;
                            ev.data.ptr = c;
                            epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                        }
                    }
                }
                else if (events[j].events & EPOLLIN && c->state == Connection::RECEIVING)
                {
                    char buf[BUFFER_SIZE];
                    ssize_t r = recv(c->fd, buf, sizeof(buf), 0);
                    if (r > 0)
                    {
                        c->recv_buf.append(buf, r);
                        // 检查是否收到回声
                        if (c->recv_buf.find("服务器已收到消息: PING") != string::npos)
                        {
                            echo_success++;
                            c->state = Connection::DONE;
                            epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                        }
                    }
                    else if (r == 0)
                    {
                        echo_failed++;
                        c->state = Connection::DONE;
                        close(c->fd);
                        c->fd = -1;
                    }
                }
            }
        }

        auto verify_end = chrono::steady_clock::now();
        auto verify_ms = chrono::duration_cast<chrono::milliseconds>(verify_end - verify_start).count();

        cout << "--- 回声验证完成 ---\n"
             << "成功: " << echo_success << "\n"
             << "失败: " << echo_failed << "\n"
             << "耗时: " << verify_ms << " ms\n\n";
    }

    // 保持连接
    if (cfg.keep_alive_sec > 0 && connected > 0)
    {
        cout << "保持 " << cfg.keep_alive_sec << " 秒连接..." << endl;
        this_thread::sleep_for(chrono::seconds(cfg.keep_alive_sec));
    }

    // 关闭所有连接
    cout << "关闭连接..." << endl;
    int closed = 0;
    for (auto &c : conns)
    {
        if (c.fd != -1)
        {
            close(c.fd);
            closed++;
        }
    }
    close(epfd);

    double total_sec = chrono::duration<double>(chrono::steady_clock::now() - start_time).count();
    cout << "\n=== 压测完成 ===\n"
         << "总连接数: " << cfg.total_connections << "\n"
         << "成功连接: " << connected << "\n"
         << "失败: " << failed << "\n"
         << "总耗时: " << total_sec << " 秒\n"
         << "已关闭: " << closed << " 个连接\n";

    return 0;
}
