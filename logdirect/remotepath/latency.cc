#include <iostream>
#include <chrono>
#include <cassert>

#include "cmdline.h"
#include "path.h"

using namespace std;

int main(int argc, char ** argv) {
    cmdline::parser p;
    p.add<int>("operation", 'o', "operation number (100000)", false, 100000);
    p.add<int>("pathtype", 'p', "remote persistent path type (1/2/3/4)", false, 1);
    p.add<bool>("machine", 't', "machine type: server or client", true);
    p.add<int>("reqsize", 's', "request size (Byte)", false, 64);

    p.parse_check(argc, argv);

    int opnum = p.get<int>("operation");
    bool is_server = p.get<bool>("machine");
    int reqsize = p.get<int>("reqsize");
    PathType path_type = (PathType) p.get<int>("pathtype");

    cerr << "operaion: " << opnum  << endl
         << "machine: " << (is_server ? "Server" : "Client") << endl
         << "reqsize: " << reqsize << endl
         << "pathtype: " << GetPath(path_type) << endl;
    
    gLength = reqsize;

    auto pair1 = RDMADevice::make_rdma(RDMA_DEVICE, RDMA_PID, RDMA_GID);
    std::unique_ptr<RDMADevice> device = std::move(pair1.first);
    if(pair1.second != Status::Ok) {
        fprintf(stderr, "open device error\n");
        exit(-1);
    }

    if(is_server == true) {
        Server * s = NewServer(device.get(), path_type, std::to_string(RDMA_PORT));
        s->Run();
        
        delete s;
    } else {
        Client * c = NewClient(device.get(), path_type, RDMA_IP, std::to_string(RDMA_PORT));
        c->Connect();
        
        char local_buf[8192];
        memset(local_buf, 77, 8192);
        assert(reqsize <= 8192);

        auto start = chrono::steady_clock::now();
        for(int i = 0; i < opnum; i++) {
            // fprintf(stderr, "%d\n", i);
            c->Write(local_buf);
        }
        auto end = chrono::steady_clock::now();
        auto dur = chrono::duration_cast<chrono::nanoseconds>(end - start);
        cout << (float) dur.count() / opnum / 1000 << " us" << endl;

        c->Close();
        delete c;
    }
    return 0;
}