//
// Created by fishti on 2026/6/14.
//

#pragma once

#ifndef MCCS3_0_CONNETION_H
#define MCCS3_0_CONNETION_H

#include "MCCS.h"

#define BUFFER_SIZE 1024

class connetion
{
    int fd;
    sockaddr_in addr;
    char readbuffer[BUFFER_SIZE];
    class WR_BUFFER{
        char buf[BUFFER_SIZE];
        int size;
    };
    
    WR_BUFFER writebuffer;
};


#endif //MCCS3_0_CONNETION_H
