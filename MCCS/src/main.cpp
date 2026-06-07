#include "MCCS.h"
#include "CTCPserver.h"
#include "CLIENT.h"
#include "ThreadPool.h"

using namespace std;

const int EPOLL_MAX = (1 << 20);

static int user_count = 0;

void setnonblocking(int);
void calc();    //模拟计算密集型任务

int main(int argc, char *argv[]) {
    if(argc != 2)
    {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        return 1;
    }

    unique_ptr<client[]> client_list = make_unique<client[]>(EPOLL_MAX);
    // client *client_list = new client[EPOLL_MAX];

    ctcpserver sv_socket(argv[1]);
    cout << "服务器已启动，等待连接..." << '\n' << endl;

    int epfd = epoll_create1(0);
    if (epfd == -1) {
        cerr << "Error creating epoll instance" << endl;
        return 1;
    }

    epoll_event ev, events[EPOLL_MAX];
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = sv_socket.socket_sever;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sv_socket.socket_sever, &ev) == -1) {
        cerr << "Error adding server socket to epoll" << endl;
        return 1;
    }

    ThreadPool<calc> pool(4);

    while(1){
        int ev_cnt = epoll_wait(epfd, events, EPOLL_MAX, -1);
        if (ev_cnt == -1) {
            cerr << "Error waiting for epoll events" << endl;
            return 1;
        }

        for(int i = 0; i < ev_cnt; i ++){
            if(events[i].data.fd == sv_socket.socket_sever ){
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(sv_socket.socket_sever, (sockaddr *)&client_addr, &client_len);
                if (client_fd == -1) {
                    cerr << "Error accepting client connection" << endl;
                    continue;
                }

                if(client_len >= EPOLL_MAX){  // 后期提示用户过多 返回给客户端
                    cerr << "Too many clients connected" << endl;
                    close(client_fd);
                    continue;
                }

                client_list[user_count++] = client(client_fd);
                ev.events = EPOLLIN | EPOLLET;
            }


        }
    }

    return 0;
}

void setnonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        cerr << "Error getting file descriptor flags" << endl;
        exit(1);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        cerr << "Error setting file descriptor to non-blocking" << endl;
        exit(1);
    }
}

class worker_thread {
public:

    using task = function<void()>;

    mutex mtx;
    condition_variable cv;
    bool has_task = false;
    atomic<bool> stop_thread = false;
    queue<function<void()>> tasks_q;



};

void calc(){
    long long sum = 0;
    for(int i = 1; i <= 500000000; i ++){
        sum += i;
    }
    cout << "计算结束，结果为: " << sum << endl;
}