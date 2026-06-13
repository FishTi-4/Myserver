#include "connection.h"

connection::connection(int fd)
{ 
    this->fd = fd;
}

connection::~connection()
{
    close(this->fd);
}

void connection::operator=(const connection& other){
    this->fd = other.fd;
    memset(this->readbuffer, 0, BUFFER_SIZE);
    memset(&this->addr, 0, sizeof(this->addr));
    this->writebuffer.size = 0;
}