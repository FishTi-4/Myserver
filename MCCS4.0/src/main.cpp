#include "MCCS.hpp"


using namespace std;

int create_listen_socket(const char *port);


struct uring_task{

    struct promise_type{
        suspend_never initiall_suspend() noexcept {return {};}
        suspend_never final_suspend() noexcept {return {};}

        void return_void() noexcept {}
        void unhandled_exception() noexcept {}

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

io_uring uring_create()
{ 
    io_uring ring{};
    io_uring_params p{};

    p.sq_thread_idle = 2000;
    p.flags = IORING_SETUP_SQPOLL | IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_DEFER_TASKRUN | IORING_SETUP_COOP_TASKRUN;
    

    if (io_uring_queue_init_params(QUEUE_DEPTH, &ring, &p) < 0) [[unlikely]] {
        cerr << "io_uring_queue_init_params() failed" << endl;
        exit(1);
    }

    return ring;
}


void MCCS(const char *port, int thread_id)
{
    
    int listen_fd = create_listen_socket(port);
    io_uring ring = uring_create();

    auto client = make_unque<
}

int main()
{
    unsigned nthreads = thread::hardware_concurrency();
    if (nthreads == 0) nthreads = 4;

    cout << "running with " << nthreads << " threads" << endl;

    vector<thread> threads;
    threads.reserve(nthreads);

    for (unsigned i = 0; i < nthreads; ++i)
    {
        threads.emplace_back(thread(MCCS), "8080", i);
    }

    for(auto& t : threads) t.join();

}



int create_listen_socket(const char *port)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if(listen_fd < 0) [[unlikely]] {
        cerr << "socket() failed" << endl;
        exit(1);
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(atoi(port));
    if (bind(listen_fd, (sockaddr *)&addr, sizeof(addr)) < 0) [[unlikely]] {
        cerr << "bind() failed" << endl;
        exit(1);
    }
    if (listen(listen_fd, Xsize) < 0) [[unlikely]] {
        cerr << "listen() failed" << endl;
        exit(1);
    }

    return listen_fd;
}