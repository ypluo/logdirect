#include "path.h"

int gLength = 0;

Server * NewServer(RDMADevice * device, PathType t, const std::string & port) {
    if(t == RDMA_PMR)
        return new Server1(device, port);
    else if(t == RDMA_PMEM)
        return new Server2(device, port);
    else if(t == RDMA_SSD)
        return new Server3(device, port);
    else if(t == RDMA_PMR0)
        return new Server4(device, port);
    else {
        fprintf(stderr, "error type\n");
        exit(-1);
    }
}

Client * NewClient(RDMADevice * device, PathType t, const std::string & ip, const std::string & port) {
    if(t == RDMA_PMR)
        return new Client1(device, ip, port);
    else if(t == RDMA_PMEM)
        return new Client2(device, ip, port);
    else if(t == RDMA_SSD)
        return new Client3(device, ip, port);
    else if(t == RDMA_PMR0)
        return new Client4(device, ip, port);
    else {
        fprintf(stderr, "error type\n");
        exit(-1);
    }
}

std::string GetPath(PathType t) {
    if(t == RDMA_PMR)
        return "RDMA_PMR";
    else if(t == RDMA_PMEM)
        return "RDMA_PMEM";
    else if(t == RDMA_SSD)
        return "RDMA_SSD";
    else if(t == RDMA_PMR0)
        return "RDMA_PMR0";
    else {
        fprintf(stderr, "error type\n");
        exit(-1);
    }
}