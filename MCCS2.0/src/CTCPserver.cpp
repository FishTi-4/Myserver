
#include "CTCPserver.h"

ctcpserver:: ctcpserver(std::string inport) : port((htons(stoi(inport))))
{
    if ((socket_sever = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
    {
        std::cerr << "Error creating socket" << std::endl;
        exit(1);
    }

    int opt = 1;
    setsockopt(socket_sever, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_sever, (sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        std::cerr << "Error binding socket" << std::endl;
        exit(1);
    }
    if (listen(socket_sever, 65535) == -1)
    {
        std::cerr << "Error listening on socket" << std::endl;
        exit(1);
    }
}

ctcpserver::~ctcpserver()
{
    close(socket_sever);
}
