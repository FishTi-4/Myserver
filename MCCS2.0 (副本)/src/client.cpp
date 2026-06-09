#include "CLIENT.h"


client::client(int fd):client_fd(fd){}

client::~client(){close(client_fd);
    outmessage.clear();
}

client::client():client_fd(-1){};

client::client(client&& other) : outmessage(std::move(other.outmessage)),
    need_write(other.need_write), buffer_get_size(other.buffer_get_size), client_fd(other.client_fd) {
    buffer = std::move(other.buffer);
    other.buffer_get_size = 0;
    other.need_write = false;
    other.client_fd = -1;
    other.outmessage.clear();
}

client& client::operator=(client&& other) {

    if (this != &other) {
        
        client_fd = other.client_fd;
        outmessage = std::move(other.outmessage);
        buffer = std::move(other.buffer);
        buffer_get_size = other.buffer_get_size;
        need_write = other.need_write;

        other.client_fd = -1;
        other.buffer_get_size = 0;
        other.need_write = false;
        other.outmessage.clear();

    }
    return *this;
}
