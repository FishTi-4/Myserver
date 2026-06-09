#include <iostream>
#include <string>
#include <vector>
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
#include <signal.h>

using namespace std;

const int MAX_EVENTS = 16384;
const int MSG_LEN = 30;

struct Conn
{
    int fd = -1;
    enum State { CONNECTING, IDLE, SENDING, RECEIVING, FAILED } state = CONNECTING;
    string send_buf;
    string recv_buf;
    size_t send_off = 0;
    uint64_t cnt = 0;
};

atomic<uint64_t> g_msgs{0};
atomic<bool> g_running{true};
atomic<int> g_connected{0};
atomic<int> g_failed{0};

string mkmsg(int cid, int seq)
{
    char b[64];
    snprintf(b, sizeof(b), "%06d-%06d-AAAAAAAAAAAAAAAA", cid % 1000000, seq % 1000000);
    return string(b, MSG_LEN);
}

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        cerr << "用法: " << argv[0] << " <host> <port> <连接数> <测试秒> [源IP数]\n";
        return 1;
    }

    string host = argv[1];
    int port = stoi(argv[2]);
    int total = stoi(argv[3]);
    int secs = stoi(argv[4]);
    int nips = max(1, (argc >= 6 ? stoi(argv[5]) : 1));

    signal(SIGPIPE, SIG_IGN);

    cout << "=== MCCS QPS 测试 ===\n"
         << "目标: " << host << ":" << port << "\n"
         << "连接数: " << total << "\n"
         << "测试时间: " << secs << "s\n"
         << "消息长度: " << MSG_LEN << "B\n"
         << "源IP数: " << nips << "\n\n";

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &srv.sin_addr);

    int epfd = epoll_create1(0);
    vector<Conn> conns(total);

    auto t0 = chrono::steady_clock::now();

    // ======== Phase 1: Establish connections ========
    cout << "[1/2] 建立连接..." << endl;

    int inflight = 0;
    int next = 0;
    auto ph1_start = chrono::steady_clock::now();

    while (g_connected + g_failed < total)
    {
        while (inflight < 5000 && next < total)
        {
            int i = next++;
            int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (fd == -1) { conns[i].state = Conn::FAILED; g_failed++; continue; }
            conns[i].fd = fd;

            sockaddr_in la{};
            la.sin_family = AF_INET;
            char lip[32];
            snprintf(lip, sizeof(lip), "127.0.0.%d", (i % nips) + 1);
            inet_pton(AF_INET, lip, &la.sin_addr);
            la.sin_port = 0;
            if (bind(fd, (sockaddr*)&la, sizeof(la)) == -1)
            {
                close(fd); conns[i].fd = -1; conns[i].state = Conn::FAILED; g_failed++;
                continue;
            }

            int ret = connect(fd, (sockaddr*)&srv, sizeof(srv));
            if (ret == -1 && errno != EINPROGRESS)
            {
                close(fd); conns[i].fd = -1; conns[i].state = Conn::FAILED; g_failed++;
                continue;
            }

            epoll_event ev{};
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.ptr = &conns[i];
            if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1)
            {
                close(fd); conns[i].fd = -1; conns[i].state = Conn::FAILED; g_failed++;
            }
            else inflight++;
        }

        epoll_event evs[MAX_EVENTS];
        int n = epoll_wait(epfd, evs, MAX_EVENTS, 100);
        if (n <= 0) continue;

        for (int j = 0; j < n; j++)
        {
            Conn *c = (Conn*)evs[j].data.ptr;
            if (c->state != Conn::CONNECTING) continue;

            int err = 0; socklen_t elen = sizeof(err);
            if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &err, &elen) == 0 && err == 0)
            {
                c->state = Conn::IDLE;
                c->send_buf = mkmsg(c->fd, 1);
                c->send_off = 0;
                g_connected++;
                inflight--;
            }
            else
            {
                epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                close(c->fd); c->fd = -1; c->state = Conn::FAILED; g_failed++;
                inflight--;
            }
        }

        auto now = chrono::steady_clock::now();
        auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(now - ph1_start).count();
        if (elapsed_ms >= 2000 && g_connected > 0)
        {
            double rate = g_connected * 1000.0 / elapsed_ms;
            cout << "\r  已连接: " << g_connected << "/" << total
                 << " (失败: " << g_failed << ") "
                 << "速率: " << (int)rate << " conn/s" << flush;
            ph1_start = now;
        }
    }

    auto ph1_end = chrono::steady_clock::now();
    auto ph1_ms = chrono::duration_cast<chrono::milliseconds>(ph1_end - t0).count();
    cout << "\r  已连接: " << g_connected << "/" << total
         << " (失败: " << g_failed << ") "
         << "耗时: " << ph1_ms << "ms"
         << " 速率: " << (g_connected * 1000.0 / max(ph1_ms, 1L)) << " conn/s\n" << endl;

    if (g_connected == 0) { cerr << "无可用连接\n"; return 1; }

    // ======== Phase 2: QPS test ========
    cout << "[2/2] 持续收发测试 (" << secs << " 秒)..." << endl;

    // Activate all idle connections for send/recv
    for (auto &c : conns)
    {
        if (c.state == Conn::IDLE)
        {
            c.state = Conn::SENDING;
            epoll_event ev{};
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.ptr = &c;
            epoll_ctl(epfd, EPOLL_CTL_MOD, c.fd, &ev);
        }
    }

    auto bench_start = chrono::steady_clock::now();
    auto last_rpt = bench_start;
    uint64_t last_cnt = 0;

    thread timer([secs]() {
        this_thread::sleep_for(chrono::seconds(secs));
        g_running = false;
    });
    timer.detach();

    while (g_running)
    {
        epoll_event evs[MAX_EVENTS];
        int n = epoll_wait(epfd, evs, MAX_EVENTS, 100);
        if (n <= 0) continue;

        for (int j = 0; j < n; j++)
        {
            Conn *c = (Conn*)evs[j].data.ptr;
            if (c->state == Conn::FAILED || c->fd == -1) continue;

            if ((evs[j].events & EPOLLOUT) && c->state == Conn::SENDING)
            {
                ssize_t s = send(c->fd, c->send_buf.c_str() + c->send_off,
                                 c->send_buf.size() - c->send_off, MSG_NOSIGNAL);
                if (s > 0)
                {
                    c->send_off += s;
                    if (c->send_off >= c->send_buf.size())
                    {
                        c->send_off = 0;
                        c->recv_buf.clear();
                        c->state = Conn::RECEIVING;
                        epoll_event ev{};
                        ev.events = EPOLLIN | EPOLLET;
                        ev.data.ptr = c;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                    }
                }
                else if (s == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    c->state = Conn::FAILED;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                    close(c->fd); c->fd = -1;
                }
            }
            else if ((evs[j].events & EPOLLIN) && c->state == Conn::RECEIVING)
            {
                char buf[4096];
                ssize_t r = recv(c->fd, buf, sizeof(buf), 0);
                if (r > 0)
                {
                    c->recv_buf.append(buf, r);
                }
                else if (r == 0 || (r == -1 && errno != EAGAIN && errno != EWOULDBLOCK))
                {
                    c->state = Conn::FAILED;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, nullptr);
                    close(c->fd); c->fd = -1;
                    continue;
                }

                // 检查是否收到完整回显（纯回显，无前缀）
                if (c->recv_buf.size() >= (size_t)MSG_LEN)
                {
                    c->cnt++;
                    g_msgs++;
                    c->send_buf = mkmsg(c->fd, c->cnt + 1);
                    c->send_off = 0;
                    c->state = Conn::SENDING;
                    epoll_event ev{};
                    ev.events = EPOLLOUT | EPOLLET;
                    ev.data.ptr = c;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
                }
            }
        }

        auto now = chrono::steady_clock::now();
        auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(now - last_rpt).count();
        if (elapsed_ms >= 1000)
        {
            uint64_t cur = g_msgs.load();
            double qps = (cur - last_cnt) * 1000.0 / elapsed_ms;
            double elapsed_sec = chrono::duration<double>(now - bench_start).count();
            cout << "  [" << (int)elapsed_sec << "s] QPS: " << (uint64_t)qps
                 << "  累计: " << cur << endl;
            last_rpt = now;
            last_cnt = cur;
        }
    }

    auto bench_end = chrono::steady_clock::now();
    double bench_sec = chrono::duration<double>(bench_end - bench_start).count();

    cout << "\n========== 结果 ==========\n"
         << "成功连接: " << g_connected << "\n"
         << "失败: " << g_failed << "\n"
         << "测试时长: " << bench_sec << "s\n"
         << "总消息数: " << g_msgs.load() << "\n"
         << "平均 QPS: " << (uint64_t)(g_msgs.load() / bench_sec) << "\n";

    for (auto &c : conns)
        if (c.fd != -1) close(c.fd);
    close(epfd);

    cout << "完成\n";
    return 0;
}
