#ifndef __GREENWAY_SOCKET_SERVER__
#define __GREENWAY_SOCKET_SERVER__

#include <memory>
#include <vector>
#include <cstdio>
#include <thread>

#include "../core.h"

using namespace RDMAUtil;
using namespace SocketUtil;

namespace frontend {

class PMemClerk {
public: 
    PMemClerk(std::unique_ptr<RDMAContext> ctx, DBType * db, int id);

    ~PMemClerk() {
        munmap(context_->write_buf, BUFSIZE);
        fprintf(stderr,"closing a clerk\n");
    }

    static void Run(std::unique_ptr<PMemClerk> clk);

private:
    std::unique_ptr<RDMAContext> context_;
    DBType * db_;
    int clerk_id_;

    uint8_t * local_buf_;
};

class PMemServer : Server {
public:
    PMemServer(MyOption opt, DBType * db);

    void Listen();

private:
    std::unique_ptr<RDMADevice> rdma_device_;

    DBType * db_;
    int clerk_num_;
    int port_;
};

} // namespace frontend

#endif // __GREENWAY_SOCKET_SERVER__