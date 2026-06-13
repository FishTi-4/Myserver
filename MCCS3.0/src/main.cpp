#include "MCCS.h"
#include "connection.h"
#include "CTCPserver.h"

#include <liburing.h>

using namespace std;

constexpr int QUEUE_DEPTH = (1 << 10);
constexpr int MAX_user = 1024;

atomic<int> cur_user = 0;

io_uring uring_create() {
    io_uring ring = {};
    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
        cerr << "Queue init failed" << endl;
        exit(1);
    }
    return ring;
}
bool read_event(io_uring* ring, connection& cli ){

    io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if(!sqe) {
        cerr << "Get SQE failed" << endl;
        return false;
    }

    cli.event = READ_EVENT;
    io_uring_prep_read(sqe, cli.fd, cli.readbuffer, BUFFER_SIZE, 0);
    io_uring_sqe_set_data(sqe, &cli);
    return true;
}

bool write_event(io_uring* ring, connection& cli){ 

    io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if(!sqe) {
        cerr << "Get SQE failed" << endl;
        return false;
    }

    cli.event = WRITE_EVENT;
    io_uring_prep_write (sqe, cli.fd, cli.writebuffer.buf, cli.writebuffer.size , 0);
    io_uring_sqe_set_data(sqe, &cli);
    return true;

}

bool accept_event(io_uring* ring, connection& listen_cli){

    io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if(!sqe){
        cerr << "Get SQE failed" << endl;
        return false;
    }

    listen_cli.event = ACCEPT_EVENT;
    socklen_t len = sizeof(listen_cli.addr);
    io_uring_prep_accept(sqe, listen_cli.fd, (sockaddr*)&listen_cli.addr, &len, 0);
    io_uring_sqe_set_data(sqe, &listen_cli);
    return true;
}

// void handle_event(io_uring* ring, ){ 
    


// }


void run_server(string port){ 
    ctcpserver server(port);
    connection listen_cli = connection(server.socket_sever);
    io_uring ring = uring_create();
    accept_event(&ring, listen_cli);

    array<connection, MAX_user> clis;

    // unique_ptr<io_uring[]> cqes(new io_uring[QUEUE_DEPTH]);
    unique_ptr<io_uring_cqe*[]> cqes(new io_uring_cqe*[QUEUE_DEPTH]);

    while(true){ 
        
        if(io_uring_submit_and_wait(&ring, 1) < 0){
            cerr << "Submit and wait failed" << endl;
            break;
        }

        int num = io_uring_peek_batch_cqe(&ring, cqes.get(), QUEUE_DEPTH);
        for(int i = 0; i < num; i ++){

            connection* cli = (struct connection *)cqes[i]->user_data;
            if(cli->event == ACCEPT_EVENT){
                if(cqes[i]->res < 0){
                    cerr << "Accept failed" << endl;
                    continue;
                }
                if(++ cur_user >= MAX_user){
                    cerr << "Too many user" << endl;
                    -- cur_user;
                    continue;
                }
                
                int client_fd = cqes[i]->res;
                clis[cur_user] = std::move(connection(client_fd));

                read_event(&ring, clis[cur_user]);
                //----
                cout << "New user " << cur_user << " from " << cli->addr.sin_addr.s_addr << ":" << cli->addr.sin_port << endl;
                accept_event(&ring, listen_cli);
            }
            else if(cli->event == READ_EVENT){
                
            }
            else if(cli->event == WRITE_EVENT){

            }
            else{
                cerr << "Unknown event" << endl;
            }

            io_uring_cqe_seen(&ring, cqes[i]);
        }

    }


}

int main(int argc, char *argv[])
{

    // if (argc != 2)
    // {
    //     cerr << "Usage: " << argv[0] << " <port>" << endl;
    //     exit(1);
    // }

    run_server("8080");
    

}