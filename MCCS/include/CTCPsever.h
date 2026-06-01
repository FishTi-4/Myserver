#ifndef CTCPSEVER_H
#define CTCPSEVER_H

#include "MCCS.h"

class ctcpsever
{
public:

    int socket_sever;
    int socket_client;
    unsigned short port;
    std::string mes;

    ctcpsever(std::string inport);
    ~ctcpsever();
};

#endif