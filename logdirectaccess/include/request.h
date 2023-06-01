#ifndef __GREENWAY_REQUEST__
#define __GREENWAY_REQUEST__

#include <cstdint>

enum Operation {PUT, GET, UPDATE, DELETE, CLOSE, SYNC, NOP};
enum RequestStatus {OK, NOTFOUND, ERROR};

struct Request {
    uint32_t op : 8;
    uint32_t key_size : 24;
    uint32_t val_size;
    char keyvalue[0];

    inline uint32_t Length() {
        return sizeof(Request) + key_size + val_size;
    }
};

struct RequestReply {
    RequestStatus status;
    uint32_t val_size;
    char value[0];
};

const int MAX_REQUEST = 8192;

#endif // __GREENWAY_REQUEST__