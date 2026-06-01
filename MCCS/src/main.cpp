#include "MCCS.h"

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

int main(int argc, char *argv[]) {

    if(argc != 2)
    {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        return 1;
    }

    ctcpsever sv_socket(argv[1]);
    cout << "服务器已启动，等待连接..." << '\n' << endl;

    
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        cerr << "Error creating epoll instance" << endl;
        return 1;
    }

    epoll_event ev, events[100];
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = sv_socket.socket_sever;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sv_socket.socket_sever, &ev) == -1) {
        cerr << "Error adding server socket to epoll" << endl;
        return 1;
    }

    while(1){

        int ev_cnt = epoll_wait(epfd, events, 100, -1);
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
            }
        }
    }


    return 0;


}