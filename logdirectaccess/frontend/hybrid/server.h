#ifndef __GREENWAY_HYBRID_SERVER__
#define __GREENWAY_HYBRID_SERVER__

#include <memory>
#include <vector>
#include <cstdio>
#include <thread>
#include <mutex>
#include <queue>
#include "libcuckoo/cuckoohash_map.h"

#include "../core.h"

using namespace RDMAUtil;
using namespace SocketUtil;
using HashType = libcuckoo::cuckoohash_map<std::string, DBIndex>;

namespace frontend {

struct Syncer {
    Syncer(uint64_t t) : done(false), tail(t) {}
    bool done;
    uint32_t tail;
};

/* HybridClerk: sync on every operation, but write do not sync to disk immediately */
class HybridClerk {
public: 
    HybridClerk(std::unique_ptr<RDMAContext> ctx, DBType * db, HashType *index, int log_fd, int id);

    ~HybridClerk() {
        fprintf(stderr,"closing a clerk\n");
    }

    static void Run(std::unique_ptr<HybridClerk> clk, std::function<void(void)> callback);

private:
    std::unique_ptr<RDMAContext> context_;
    DBType * db_;
    uint8_t * send_buf_;
    uint8_t * write_buf_;
    uint32_t buf_tail_; // buffer tail will be pushed to client
                        // buffer head is stored in local_buf_[0] and writen by client
    int log_fd_;
    HashType * im_index_;

    std::mutex mu_;
    std::queue<Syncer *> syncers_;

    int clerk_id_;
};

class HybridServer : Server {
public:
    HybridServer(MyOption opt, DBType * db);

    void Listen();

private:
    std::unique_ptr<RDMADevice> rdma_device_;
    DBType * db_;
    int clerk_num_;
    int port_;
    std::string path_;
    HashType map_;

    #ifdef DMABUF
    int dmabuf_fd_;
    uint64_t bitmap_;
    #endif
};

} // namespace frontend

#endif // __GREENWAY_HYBRID_SERVER__