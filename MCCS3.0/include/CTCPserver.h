#pragma once

#ifndef CTCPSEVER_H
#define CTCPSEVER_H

#include "MCCS.h"

class ctcpserver
{
public:

    int socket_sever;
    unsigned short port;

    ctcpserver(std::string inport);
    ~ctcpserver();
};

#endif