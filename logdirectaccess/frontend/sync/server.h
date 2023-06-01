#ifndef __GREENWAY_SYNC_SERVER__
#define __GREENWAY_SYNC_SERVER__

#include <memory>
#include <vector>
#include <cstdio>
#include <thread>

#include "../core.h"

using namespace RDMAUtil;
using namespace SocketUtil;

namespace frontend {

class SyncClerk {
public: 
    SyncClerk(std::unique_ptr<RDMAContext> ctx, DBType * db, int id);

    ~SyncClerk() {
        fprintf(stderr,"closing a clerk\n");
    }

    static void Run(std::unique_ptr<SyncClerk> clk, std::unique_ptr<uint8_t[]> buf);

private:
    std::unique_ptr<RDMAContext> context_;
    DBType * db_;
    uint8_t * local_buf_;

    int clerk_id_;
};

class SyncServer : Server {
public:
    SyncServer(MyOption opt, DBType * db);

    void Listen();

private:
    std::unique_ptr<RDMADevice> rdma_device_;
    DBType * db_;
    int clerk_num_;
    int port_;
};

} // namespace frontend

#endif // __GREENWAY_SYNC_SERVER__