#include "MCCS.h"
#include "CTCPserver.h"

using namespace std;

int main(int argc, char *argv[]) {

    if(argc != 2)
    {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        return 1;
    }

    ctcpserver sv_socket(argv[1]);
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