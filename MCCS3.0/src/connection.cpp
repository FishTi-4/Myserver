#pragma once

#include "connection.h"

connection::connection(int fd)
{ 
    this->fd = fd;
        memset(this->readbuffer, 0, BUFFER_SIZE);
        memset(&this->addr, 0, sizeof(this->addr));
        this->writebuffer.size = 0;
}

connection::~connection()
{
    close(this->fd);
}