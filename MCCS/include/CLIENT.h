#include "MCCS.h"

class client{
public:

    client(int fd);
    client();
    ~client();
    client(client& ) = delete;
    client& operator=(const client&) = delete;

    client(client&&);
    client& operator=(client&&);

    char* outmessage;
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(BUFFER_SIZE);
    // char buffer[BUFFER_SIZE];
    int client_fd;
};