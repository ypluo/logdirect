#ifndef __GREENWAY_CORE__
#define __GREENWAY_CORE__

#include <string>
#include <cstdint>
#include "rdmautil/rdmautil.h"
#include "rdmautil/socketutil.h"

#include "flags.h"
#include "db.h"
#include "request.h"

namespace frontend {
class Server {
public:
    Server() {}

    virtual void Listen() = 0;
};

class Client {
public:
    Client() {}

    virtual void Connect() = 0;

    virtual void SendPut(const char * key, const char * val) = 0;

    virtual void SendUpdate(const char * key, const char * val) = 0;

    virtual bool SendGet(const char * key, std::string * val) = 0;

    virtual bool SendDelete(const char * key) = 0;

    virtual void SendClose() = 0;

    virtual int GetClientID() = 0; 

    virtual void SendSync() = 0;
};

const int MAX_ASYNC_SIZE   = 128 * 1024;
const uint32_t CLIENT_DONE = 0x7f7f7f7f;
const uint32_t CLERK_DONE  = 0xf7f7f7f7;
const int BATCH_SIZE       = 16 * 1024;
const int RING_HEADER      = sizeof(uint32_t) * 2;

} // namespace frontend

#endif // __GREENWAY_CORE__