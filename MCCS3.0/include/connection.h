//
// Created by fishti on 2026/6/14.
//

#pragma once

#ifndef MCCS3_0_CONNECTION_H
#define MCCS3_0_CONNECTION_H

#include "MCCS.h"

#define BUFFER_SIZE 1024

enum EVENT_TYPE {
    ACCEPT_EVENT = 1,
    READ_EVENT,
    WRITE_EVENT
};

class WR_BUFFER{
    public:
    char buf[BUFFER_SIZE];
    int size;
};

class connection
{
public:
    int fd;
    sockaddr_in addr;
    char readbuffer[BUFFER_SIZE];
    

    connection();
    connection(int fd);
    ~connection();

    connection& operator=(connection&& other);

    EVENT_TYPE event;
    WR_BUFFER writebuffer;

};



#endif //MCCS3_0_CONNETION_H
