#include <string>
#include "socketutil.h"

using namespace SocketUtil;

int main(int argc, char ** argv) {
    char msg[100] = {0};
    
    if(argc == 1) {
        auto socket = Socket::make_socket(SERVER, 8080);
        memcpy(msg, "hello client", 12);
        socket->Send(msg, 12);
        printf("Send: %s\n", msg);
    } else {
        std::string ipaddr = argv[1];
        auto socket = Socket::make_socket(CLIENT, 8080, ipaddr);
        
        socket->Recv(msg, 12);
        printf("Recv: %s\n", msg);
    }

    return 0;
}