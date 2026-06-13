#include "connection.h"

connection::connection(int fd)
{ 
    this->fd = fd;
}

connection::~connection()
{
    close(this->fd);
}

connection::connection() noexcept { 
    fd = -1;
}

connection& connection::operator=(connection&& other) noexcept {
    if (this == &other) return *this;
    // if (fd != -1) {
    //     close(fd);
    // }

    fd = other.fd;
    memset(readbuffer, 0, BUFFER_SIZE);
    
    // memcpy(readbuffer, other.readbuffer, BUFFER_SIZE);
    
    addr = other.addr;
    writebuffer.size = 0;
    // writebuffer.size = other.writebuffer.size;

    other.fd = -1;
    // memset(other.readbuffer, 0, BUFFER_SIZE);
    // memset(&other.addr, 0, sizeof(other.addr));
    // other.writebuffer.size = 0;

    return *this;
}