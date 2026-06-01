#include "Serve.h"

using namespace std;

class ctcpsever
{
public:
    int socket_sever;
    int socket_client;
    unsigned short port;
    string mes;
    ctcpsever(string inport) : port((htons(stoi(inport))))
    {
        if ((socket_sever = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
        {
            cerr << "Error creating socket" << endl;
            exit(1);
        }
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = port;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(socket_sever, (sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            cerr << "Error binding socket" << endl;
            exit(1);
        }
        if (listen(socket_sever, 5) == -1)
        {
            cerr << "Error listening on socket" << endl;
            exit(1);
        }
    }

    ~ctcpsever()
    {
        close(socket_sever);
    }
};

class get_date
{
    
    map<int, string> recv_buffer_mp;
    map<int, array<char, 1024>> buf_mp;

public:
    int gt_lin(int scc, string &mes)
    {
        int socket_fd = scc;
        auto &recv_buffer = recv_buffer_mp[socket_fd];
        auto &buf = buf_mp[socket_fd];

        while (1)
        {
            int p = recv_buffer.find('\n');
            if (p != string::npos)
            {
                mes = recv_buffer.substr(0, p);
                recv_buffer.erase(0, p + 1);
                return 1;
            }

            int n = recv(socket_fd, &buf[0], sizeof(buf) - 1, 0);
            if (n < 0)
            {
                if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
                {
                    return 0;
                }
                cerr << "Error receiving data" << endl;

                recv_buffer_mp.erase(socket_fd);
                buf_mp.erase(socket_fd);
                close(socket_fd);

                return 2;
            }
            else if (n == 0)
            {
                mes = std::move(recv_buffer);
                cerr << "Connection closed by client: " << socket_fd << endl;

                recv_buffer_mp.erase(socket_fd);
                buf_mp.erase(socket_fd);
                close(socket_fd);

                return 2;
            }
            else
            {
                recv_buffer.append(buf.data(), n);
            }
        }
    }
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cout << "Usage: " << argv[0] << " <port>" << endl;
        return 1;
    }
    ctcpsever server(argv[1]);
    cout << "服务器已启动，等待连接..." << '\n'
         << endl;

    get_date gt;
    epoll_event ev, events[10];

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        cerr << "Error creating epoll instance" << endl;
        return 1;
    }

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server.socket_sever;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server.socket_sever, &ev);

    while (true)
    {
        int nfds = epoll_wait(epoll_fd, events, 10, -1);
        for (int i = 0; i < nfds; ++i)
        {
            if (events[i].data.fd == server.socket_sever)
            {
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);

                int client_fd = accept4(server.socket_sever, (sockaddr *)&client_addr, &client_len, SOCK_NONBLOCK);

                if (client_fd == -1)
                {
                    cerr << "Error accepting connection" << endl;
                    continue;
                }

                ev.data.fd = client_fd;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                cout << "新连接来自: " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << " socket: " << client_fd << '\n'
                     << endl;
            }
            else
            {

                int soc = events[i].data.fd;
                string mes;

                if (int sign = gt.gt_lin(soc, mes))
                {
                    // cout << sign << endl;
                    if (sign == 2)
                    {
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, soc, nullptr);
                        continue;
                    }
                    else if (sign == 0)
                    {
                        continue;
                    }

                    cout << "收到消息: " << mes << endl;
                    string response = "服务器已收到消息: " + mes + "\n";
                    send(soc, response.c_str(), response.size(), 0);
                }
            }
        }
    }

    cout << "服务器已关闭" << endl;

    return 0;
}