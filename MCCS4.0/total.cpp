// MCCS3.0 - io_uring coroutine HTTP server (single-file)
// Build: g++ -std=gnu++23 -O3 -march=native -flto total.cpp -o MCCS -luring

#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <cstring>
#include <cerrno>
#include <atomic>
#include <vector>
#include <algorithm>
#include <coroutine>
#include <utility>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <liburing.h>
#include <pthread.h>
#include <liburing.h>

using namespace std;

constexpr int QUEUE_DEPTH = 2048;
constexpr int MAX_USER   = (1 << 20);
constexpr int BUF_SIZE   = 128;

atomic<int> cur_user = 0;

static constexpr const char HTTP_OK[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 5\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "hello";
static constexpr int HTTP_OK_LEN = sizeof(HTTP_OK) - 1;

struct uring_awaitable_base {
    int result = 0;
    coroutine_handle<> handle;
};

struct uring_read final : uring_awaitable_base {
    io_uring* ring; int fd; char* buf; __u32 len;
    uring_read(io_uring* r, int f, char* b, __u32 l) : ring(r), fd(f), buf(b), len(l) {}
    bool await_ready() noexcept { return false; }
    void await_suspend(coroutine_handle<> h) noexcept {
        handle = h;
        io_uring_sqe* sqe = io_uring_get_sqe(ring);
        if (!sqe) [[unlikely]] { result = -ECANCELED; h.resume(); return; }
        io_uring_prep_read(sqe, fd, buf, len, 0);
        io_uring_sqe_set_data(sqe, this);
    }
    int await_resume() noexcept { return result; }
};

struct uring_write final : uring_awaitable_base {
    io_uring* ring; int fd; const char* buf; __u32 len;
    uring_write(io_uring* r, int f, const char* b, __u32 l) : ring(r), fd(f), buf(b), len(l) {}
    bool await_ready() noexcept { return false; }
    void await_suspend(coroutine_handle<> h) noexcept {
        handle = h;
        io_uring_sqe* sqe = io_uring_get_sqe(ring);
        if (!sqe) [[unlikely]] { result = -ECANCELED; h.resume(); return; }
        io_uring_prep_write(sqe, fd, buf, len, 0);
        io_uring_sqe_set_data(sqe, this);
    }
    int await_resume() noexcept { return result; }
};

struct uring_accept final : uring_awaitable_base {
    io_uring* ring; int listen_fd; sockaddr_in* addr; socklen_t* addrlen;
    uring_accept(io_uring* r, int lfd, sockaddr_in* a, socklen_t* al)
        : ring(r), listen_fd(lfd), addr(a), addrlen(al) {}
    bool await_ready() noexcept { return false; }
    void await_suspend(coroutine_handle<> h) noexcept {
        handle = h;
        io_uring_sqe* sqe = io_uring_get_sqe(ring);
        if (!sqe) [[unlikely]] { result = -ECANCELED; h.resume(); return; }
        io_uring_prep_accept(sqe, listen_fd, (sockaddr*)addr, addrlen, 0);
        io_uring_sqe_set_data(sqe, this);
    }
    int await_resume() noexcept { return result; }
};

struct uring_task {
    struct promise_type {
        suspend_never initial_suspend() noexcept { return {}; }
        suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept { terminate(); }
        uring_task get_return_object() noexcept {
            return uring_task{coroutine_handle<promise_type>::from_promise(*this)};
        }
    };
    coroutine_handle<promise_type> coro;
    uring_task() noexcept : coro(nullptr) {}
    explicit uring_task(coroutine_handle<promise_type> h) noexcept : coro(h) {}
    uring_task(const uring_task&) = delete;
    uring_task(uring_task&& o) noexcept : coro(exchange(o.coro, nullptr)) {}
    uring_task& operator=(uring_task&& o) noexcept {
        if (this != &o) { if (coro) coro.destroy(); coro = exchange(o.coro, nullptr); }
        return *this;
    }
    ~uring_task() { if (coro) coro.destroy(); }
};

io_uring uring_create() {
    io_uring ring{};
    io_uring_params p{};
    p.flags = IORING_SETUP_SQPOLL | IORING_SETUP_SINGLE_ISSUER
            | IORING_SETUP_DEFER_TASKRUN | IORING_SETUP_COOP_TASKRUN;
    p.sq_thread_idle = 2000;
    if (io_uring_queue_init_params(QUEUE_DEPTH, &ring, &p) == 0) return ring;

    io_uring ring2{};
    p = {};
    p.flags = IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_DEFER_TASKRUN
            | IORING_SETUP_COOP_TASKRUN;
    if (io_uring_queue_init_params(QUEUE_DEPTH, &ring2, &p) == 0) return ring2;

    io_uring ring3{};
    if (io_uring_queue_init(QUEUE_DEPTH, &ring3, 0) < 0) {
        cerr << "Queue init failed: " << strerror(errno) << endl; exit(1);
    }
    return ring3;
}

int create_listen_fd(const string& port) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd == -1) [[unlikely]] { cerr << "socket() failed" << endl; exit(1); }
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(stoi(port));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) == -1) [[unlikely]] {
        cerr << "bind() failed" << endl; exit(1);
    }
    if (listen(fd, 65535) == -1) [[unlikely]] {
        cerr << "listen() failed" << endl; exit(1);
    }
    return fd;
}

uring_task handle_client(io_uring* ring, int client_fd) {
    char buf[BUF_SIZE];
    int opt = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    setsockopt(client_fd, IPPROTO_TCP, 12 /*TCP_QUICKACK*/, &opt, sizeof(opt));

    for (;;) {
        int n = co_await uring_read(ring, client_fd, buf, BUF_SIZE);
        if (n <= 0) break;

        if (n >= 4) {
            __u32 m; memcpy(&m, buf, 4);
            if (m == 0x20544547 || m == 0x54534f50 || m == 0x44414548) {
                ssize_t w = write(client_fd, HTTP_OK, HTTP_OK_LEN);
                if (w >= 0) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    w = co_await uring_write(ring, client_fd, HTTP_OK, HTTP_OK_LEN);
                    if (w >= 0) continue;
                }
                break;
            }
        }
        int w = co_await uring_write(ring, client_fd, buf, (__u32)n);
        if (w < 0) break;
    }
    close(client_fd);
    --cur_user;
    co_return;
}

uring_task listen_loop(io_uring* ring, int listen_fd, uring_task* clients) {
    for (;;) {
        sockaddr_in addr{}; socklen_t addrlen = sizeof(addr);
        int client_fd = co_await uring_accept(ring, listen_fd, &addr, &addrlen);
        if (client_fd < 0) continue;
        if (++cur_user >= MAX_USER || client_fd >= MAX_USER) {
            close(client_fd); --cur_user; continue;
        }
        clients[client_fd] = handle_client(ring, client_fd);
    }
}

void worker_thread(const string& port, int core_id) {
    cpu_set_t cpuset; CPU_ZERO(&cpuset); CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    int listen_fd = create_listen_fd(port);
    io_uring ring = uring_create();

    auto clients = make_unique<uring_task[]>(MAX_USER);
    uring_task listener = listen_loop(&ring, listen_fd, clients.get());

    while (true) {
        int ret = io_uring_submit_and_wait(&ring, 1);
        if (ret < 0) [[unlikely]] {
            if (ret == -EINTR) continue;
            break;
        }
        unsigned head, count = 0;
        struct io_uring_cqe* cqe;
        io_uring_for_each_cqe(&ring, head, cqe) {
            auto* op = static_cast<uring_awaitable_base*>(io_uring_cqe_get_data(cqe));
            op->result = cqe->res;
            op->handle.resume();
            if (++count >= 256) break;
        }
        io_uring_cq_advance(&ring, count);
    }
}

int main() {
    unsigned nthreads = thread::hardware_concurrency();
    if (nthreads == 0) nthreads = 4;
    cout << "MCCS3.0  " << nthreads << " workers  port 8080" << endl;
    vector<thread> threads; threads.reserve(nthreads);
    for (unsigned i = 0; i < nthreads; i++)
        threads.emplace_back(worker_thread, "8080", i);
    for (auto& t : threads) t.join();
}
