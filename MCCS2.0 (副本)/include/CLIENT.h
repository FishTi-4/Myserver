#ifndef CLIENT_H
#define CLIENT_H

#include "MCCS.h"

class client{
public:

    client(int fd);
    client();
    ~client();
    client(client& ) = delete;
    client& operator=(const client&) = delete;

    client(client&&);
    client& operator=(client&&);

    // int out_size;
    // char* outmessage;
    std::string outmessage;

    bool need_write = false;
    int buffer_get_size = 0;
    
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(BUFFER_SIZE);
    // char buffer[BUFFER_SIZE];
    int client_fd;
};

#endif