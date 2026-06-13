#pragma once

#include "MCCS.h"
#include "connection.h"

#include <liburing.h>

using namespace std;

constexpr int QUEUE_DEPTH = (1 << 10);
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





int main(int argc, char *argv[])
{

    if (argc != 2)
    {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        exit(1);
    }


    

}