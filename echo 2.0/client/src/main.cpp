#include "NET.h"

using namespace std;

class ctcpclient{
public:
    int socket_fd;
    string ip;
    string port;

    ctcpclient(string inip, string inport) : ip(inip), port(inport) {
        if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            cerr << "Failed to create socket" << endl;
            exit(1);
        }
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(stoi(port));

        if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0)
        {
            cerr << "Invalid IP address" << endl;
            exit(1);
        }
        cerr << "Connecting to server " << ip << ":" << port << "..." << endl;

        if(connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            cerr << "Failed to connect to server" << endl;
            exit(1);
        }
    }

    ~ctcpclient(){
        close(socket_fd);
    }

    string recv_buffer;
    char buf[1024];
    bool gt_lin(string& mes){
        while(1){
            int p = recv_buffer.find('\n');
            if(p != string::npos){
                mes = recv_buffer.substr(0, p);
                recv_buffer.erase(0, p + 1);
                return 1;
            }
            int n = recv(socket_fd, buf, sizeof(buf) - 1, 0);
            if (n < 0) {
                cerr << "Failed to receive message from server" << endl;
                return 0;
            }
            else if(n == 0) {
                cerr << "Server closed the connection" << endl;
                if(!recv_buffer.empty()){
                    mes = std::move(recv_buffer);
                    return 1;
                }
                
                cerr << "Failed to receive message from server" << endl;
                return 0;
            }
            else{
                recv_buffer.append(buf, n);
            }
        }
    }
};

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        cerr << "Usage: " << argv[0] << " <IP_ADDRESS> <PORT>" << endl;
        return 1;
    }

    ctcpclient client(argv[1], argv[2]);
    string message;
    cout << "exit 退出" << endl;

    while (true){
        cin >> message;
        if (message == "exit") {
            break;
        }
        message += "\n";

        write(client.socket_fd, &message[0], message.size());
        if(client.gt_lin(message) == 0){
            break;
        }

        cout << "服务器返回: " << message << endl;

    }

    cout << "连接已关闭" << endl;
    return 0;
}