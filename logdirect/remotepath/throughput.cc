/*
    CopyRight (c) Luo Yongping
*/

#include <random>
#include <cstdio>
#include <thread>

#include "cmdline.h"
#include "path.h"


using namespace RDMAUtil;
using namespace std;

void run_test_client(Client * cli, int opnum) {
    char local_buf[8192];
    for(int i = 0; i < opnum; i++) {
        // fprintf(stderr, "%d\n", i);
        cli->Write(local_buf);
    }
    cli->Close();
}

int main(int argc, char ** argv) {
    cmdline::parser p;
    p.add<int>("operation", 'o', "operation number (100000)", false, 100000);
    p.add<bool>("machine", 't', "machine type: server or client", true);
    p.add<int>("reqsize", 's', "request size (Bytes)", false, 64);
    p.add<int>("thread", 'n', "number of threads (4)", false, 1);
    p.add<int>("pathtype", 'p', "remote persistent path type (1/2/3/4)", false, 1);

    p.parse_check(argc, argv);

    int opnum = p.get<int>("operation");
    bool is_server = p.get<bool>("machine");
    int reqsize = p.get<int>("reqsize");
    int thread_num = p.get<int>("thread");
    PathType path_type = (PathType) p.get<int>("pathtype");

    cerr << "operaion: " << opnum  << endl
         << "machine: " << (is_server ? "Server" : "Client") << endl
         << "thread: " << thread_num << endl
         << "reqsize: " << reqsize << endl
         << "pathtype: " << GetPath(path_type) << endl;

    gLength = reqsize;

    auto pair1 = RDMADevice::make_rdma(RDMA_DEVICE, RDMA_PID, RDMA_GID);
    std::unique_ptr<RDMADevice> device = std::move(pair1.first);
    if(is_server) {
        Server * sers[thread_num];
        std::thread tests[thread_num];
        // make connections
        for(int i = 0; i < thread_num; i++) {
            sers[i] = NewServer(device.get(), path_type, std::to_string(RDMA_PORT + i));
            tests[i] = std::thread([](Server * s){s->Run();}, sers[i]);
        }

        for(int i = 0; i < thread_num; i++) {
            tests[i].join();
            delete sers[i];
        }
    } else {
        Client * clis[thread_num];
        // make connections
        for(int i = 0; i < thread_num; i++) {
            clis[i] = NewClient(device.get(), path_type, RDMA_IP, std::to_string(RDMA_PORT + i));
            clis[i]->Connect();
            fprintf(stderr, "client %d connected\n", i);
            usleep(100000); // wait for other server 
        }

        int opnum_per_thread = opnum / thread_num;
        std::thread tests[thread_num];
        auto start = chrono::steady_clock::now();
        for(int i = 0; i < thread_num; i++) {
            tests[i] = std::thread(run_test_client, clis[i], opnum_per_thread);
        }

        for(int i = 0; i < thread_num; i++) {
            tests[i].join();
        }
        auto end = chrono::steady_clock::now();

        auto dur = chrono::duration_cast<chrono::microseconds>(end - start);
        cout << opnum * gLength / dur.count() << " MB/s" << endl;

        for(int i = 0; i < thread_num; i++) {
            delete clis[i];
        }
    }

    return 0;
}