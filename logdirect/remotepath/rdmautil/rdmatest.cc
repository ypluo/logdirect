#include "rdma_util.hpp"
#include "../socketutil/socketutil.h"

#include <memory>

using namespace RDMAUtil;
using namespace SocketUtil;

int main(int argc, char ** argv) {
    char msg[100] = {0};
    
    auto [rdma_device, device_status] = RDMADevice::make_rdma("mlx5_0", 1, 3);
    if(device_status != Status::Ok) {
        decode_rdma_status(device_status);
    }

    char * mem = new char[4096]; // memory to register
    auto [rdma_context, context_status] = rdma_device->open(mem, 4096, 20);
    if(context_status != Status::Ok) {
        decode_rdma_status(device_status);
    }

    if(argc == 1) { // server
        auto new_socket = Socket::make_socket(SERVER, 8080);
        int socket_fd = new_socket->GetSockID();
        rdma_context->default_connect(socket_fd);

        sleep(1); // wait for the client to finish write
        printf("Recv: %s\n", mem);
    } else {
        // client scenario: give ip address as the first param
        std::string ipaddr = argv[1];
        auto new_socket = Socket::make_socket(CLIENT, 8080, ipaddr);
        int socket_fd = new_socket->GetSockID();
        rdma_context->default_connect(socket_fd);

        memcpy(mem, "1234567890\0", 11);
        rdma_context->post_write((uint8_t *)mem, 11, 0, 0);
        printf("write:%s\n", mem);
    }

    delete mem;
    return 0;
}