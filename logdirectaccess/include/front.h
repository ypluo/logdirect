#ifndef __GREENWAY_FRONT__
#define __GREENWAY_FRONT__

#include "flags.h"

#include "../frontend/pmem/client.h"
#include "../frontend/sync/client.h"
#include "../frontend/group/client.h"
#include "../frontend/hybrid/client.h"

#include "../frontend/pmem/server.h"
#include "../frontend/sync/server.h"
#include "../frontend/hybrid/server.h"
#include "../frontend/group/server.h"

using namespace frontend;

inline Client * NewClient(MyOption opt, int client_id) {
    if(opt.front_type == "pmem")
        return (Client *)(new PMemClient(opt, client_id));
    else if(opt.front_type == "sync")
        return (Client *)(new SyncClient(opt, client_id));
    else if(opt.front_type == "hybrid")
        return (Client *)(new HybridClient(opt, client_id));
    else if(opt.front_type == "group")
        return (Client *)(new GroupClient(opt, client_id));
    else
        fprintf(stderr, "wrong front type\n");
    return nullptr;
}

inline Server * NewServer(MyOption opt, DBType * db) {
    if(opt.front_type == "pmem")
        return (Server *)(new PMemServer(opt, db));
    else if(opt.front_type == "sync")
        return (Server *)(new SyncServer(opt, db));
    else if(opt.front_type == "hybrid")
        return (Server *)(new HybridServer(opt, db));
    else if(opt.front_type == "group")
        return (Server *)(new GroupServer(opt, db));
    else
        fprintf(stderr, "wrong front type\n");
    return nullptr;
}

#endif // __GREENWAY_FRONT__