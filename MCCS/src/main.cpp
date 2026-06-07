#include "MCCS.h"
#include "CTCPserver.h"
#include "CLIENT.h"
#include "ThreadPool.h"

using namespace std;

const int EPOLL_MAX = (1 << 20);
static int user_count = 0;

void setnonblocking(int);
void calc(); // 模拟计算密集型任务

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        return 1;
    }

    unique_ptr<client[]> client_list = make_unique<client[]>(EPOLL_MAX);

    ctcpserver sv_socket(argv[1]);
    cout << "服务器已启动，等待连接..." << '\n'
         << endl;

    int epfd = epoll_create1(0);
    if (epfd == -1)
    {
        cerr << "Error creating epoll instance" << endl;
        return 1;
    }

    epoll_event ev;
    auto events = make_unique<epoll_event[]>(EPOLL_MAX);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = sv_socket.socket_sever;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sv_socket.socket_sever, &ev) == -1)
    {
        cerr << "Error adding server socket to epoll" << endl;
        return 1;
    }

    ThreadPool pool(128);

    while (1)
    {
        int ev_cnt = epoll_wait(epfd,  events.get() , EPOLL_MAX, -1);
        if (ev_cnt == -1)
        {
            cerr << "Error waiting for epoll events" << endl;
            return 1;
        }

        for (int i = 0; i < ev_cnt; i++)
        {
            if (events[i].data.fd == sv_socket.socket_sever)
            {
                while (1)
                {
                    sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept4(sv_socket.socket_sever, (sockaddr *)&client_addr, &client_len, SOCK_NONBLOCK);
                    if (client_fd == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        cerr << "Error accepting client connection" << endl;
                        break;
                    }

                    if (client_fd >= EPOLL_MAX)
                    { // 后期提示用户过多 返回给客户端
                        cerr << "Too many clients connected" << endl;
                        close(client_fd);
                        continue;
                    }

                    client_list[client_fd] = client(client_fd);
                    user_count++;
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = client_fd;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                        cerr << "Error adding client socket to epoll" << endl;
                        close(client_fd);
                        user_count--;
                        continue;
                    }
                    cout << "新客户端连接，fd: " << client_fd << ", 当前在线人数: " << user_count << endl;
                }
            }
            else if (events[i].events & EPOLLOUT)
            {

                int fd = events[i].data.fd;
                client &cur_client = client_list[fd];

                while(1){
                    int bytes_sent = send(fd, cur_client.outmessage.c_str(), cur_client.outmessage.size(), 0);
                    if (bytes_sent == -1)
                    {   
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break;
                        }
                        else
                        {
                            cerr << "Error sending to client fd: " << fd << endl;
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                            close(fd);
                            user_count--;
                            break;
                        }
                    }
                    else
                    {
                        cur_client.outmessage.erase(0, bytes_sent);
                        if (cur_client.outmessage.empty())
                        {
                            ev.events = EPOLLIN | EPOLLET;
                            epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
                            break;
                        }
                    }
                }

            }
            else
            {
                int fd = events[i].data.fd;
                client &cur_client = client_list[fd];

                while (1)
                {
                    if (cur_client.buffer_get_size == BUFFER_SIZE)
                        break;
                    int bytes_read = recv(fd, cur_client.buffer.get() + cur_client.buffer_get_size, BUFFER_SIZE - cur_client.buffer_get_size, 0);
                    
                    if (bytes_read > 0)
                    {
                        cur_client.buffer_get_size += bytes_read;
                    }
                    else if (bytes_read == 0)
                    {
                        if (cur_client.buffer_get_size > 0)
                            break;
                        cout << "客户端断开连接，fd: " << fd << endl;
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                        close(fd);
                        user_count--;
                        break;
                    }
                    else
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break;
                        }
                        else
                        {

                            cerr << "Error reading from client fd: " << fd << endl;
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                            close(fd);
                            user_count--;
                            break;
                        }
                    }
                }
                if (cur_client.buffer_get_size > 0)
                {
                    if (cur_client.buffer[0] == '/')
                    {
                        // 模拟计算密集型任务
                        pool.post(calc);
                        cur_client.buffer_get_size = 0;
                        ev.events = EPOLLIN | EPOLLET;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
                    }
                    else
                    {
                        // 暂时以回声验证通讯
                        cout << "收到客户端消息，fd: " << fd << ", 消息内容: " << string(cur_client.buffer.get(), cur_client.buffer_get_size) << endl;
                        cur_client.outmessage = "服务器已收到消息: " + string(cur_client.buffer.get(), cur_client.buffer_get_size);
                        cur_client.buffer_get_size = 0;

                        ev.events = EPOLLOUT | EPOLLET;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
                    }
                }
            }
        }
    }
    return 0;
}

void setnonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        cerr << "Error getting file descriptor flags" << endl;
        exit(1);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        cerr << "Error setting file descriptor to non-blocking" << endl;
        exit(1);
    }
}

void setblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        cerr << "Error getting file descriptor flags" << endl;
        exit(1);
    }
    if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1)
    {
        cerr << "Error setting file descriptor to blocking" << endl;
        exit(1);
    }
}

void calc()
{
    long long sum = 0;
    for (int i = 1; i <= 500000000; i++)
    {
        sum += i;
    }
    cout << "计算结束，结果为: " << sum << endl;
}