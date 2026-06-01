
#include "CTCPsever.h"

ctcpsever:: ctcpsever(std::string inport) : port((htons(stoi(inport))))
{
    if ((socket_sever = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
    {
        std::cerr << "Error creating socket" << std::endl;
        exit(1);
    }
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_sever, (sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        std::cerr << "Error binding socket" << std::endl;
        exit(1);
    }
    if (listen(socket_sever, 5) == -1)
    {
        std::cerr << "Error listening on socket" << std::endl;
        exit(1);
    }
}

ctcpsever::~ctcpsever()
{
    close(socket_sever);
}
