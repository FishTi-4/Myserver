#include "CLIENT.h"

client::client(int fd):client_fd(fd){}
client::~client(){close(client_fd);
    delete[] outmessage;
}
client::client() = default;

client::client(client&& other) noexcept : client_fd(other.client_fd), outmessage(other.outmessage), buffer(std::move(other.buffer)) {
    other.client_fd = -1;
    other.outmessage = nullptr;
}
client& client::operator=(client&& other) noexcept {

    if (this != &other) {
        
        client_fd = other.client_fd;
        outmessage = other.outmessage;
        buffer = std::move(other.buffer);

        other.client_fd = -1;
        other.outmessage = nullptr;

    }
    return *this;
}
    
