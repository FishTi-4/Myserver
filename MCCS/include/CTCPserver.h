#ifndef CTCPSEVER_H
#define CTCPSEVER_H

#include "MCCS.h"

class ctcpserver
{
public:

    int socket_sever;
    int socket_client;
    unsigned short port;
    std::string mes;

    ctcpserver(std::string inport);
    ~ctcpserver();
};

#endif