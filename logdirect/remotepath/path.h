#pragma once

#include <string>
#include <libpmem.h>

#include "rdmautil/rdma_util.hpp"
#include "socketutil/socketutil.h"
#include "util.h"
#include "flag.h"

using namespace RDMAUtil;
using namespace SocketUtil;

extern int gLength;

class Server {
protected:
    std::string port_;

public:
    Server(const std::string & port) : port_(port) {}
    virtual void Run() = 0;
};

class Client {
protected:
    std::string ip_;
    std::string port_;

public:
    Client(const std::string & ip, const std::string & port) : ip_(ip), port_(port) {}
    virtual bool Connect() = 0;
    virtual void Write(char * buffer) = 0;
    virtual void Close() = 0;
};

enum PathType {RDMA_PMR = 1, RDMA_PMEM, RDMA_SSD, RDMA_PMR0};
Server * NewServer(RDMADevice * device, PathType t, const std::string & port);
Client * NewClient(RDMADevice * device, PathType t, const std::string & ip, const std::string & port);
std::string GetPath(PathType t);

// RDMA to PMR directly
class Server1 : public Server {
private:
    std::unique_ptr<RDMAContext> context_;
    std::unique_ptr<Socket> socket_;

public:
    Server1(RDMADevice * device, const std::string & port) : Server(port) {
        #ifdef DMABUF
            int dmabuf_fd = mapcmb(NVME_DEVICE, BUFSIZE);
            if(dmabuf_fd <= 0) {
                fprintf(stderr, "open dmabuf error\n");
                exit(-1);
            }
            auto pair2 = device->open(dmabuf_fd, BUFSIZE, 20);
        #else
            char * tmp = new char[BUFSIZE];
            auto pair2 = device->open(tmp, BUFSIZE, 20);
        #endif
        context_ = std::move(pair2.first);
        if(pair2.second != Status::Ok) {
            fprintf(stderr, "open context error\n");
            exit(-1);
        }
        socket_ = std::move(Socket::make_socket(SERVER, stoi(port)));
        int socket_fd = socket_->GetSockID();
        context_->default_connect(socket_fd);
    }

    ~Server1() {
        #ifndef DMABUF
            delete [] (char *) context_->buf;
        #endif
    }

    void Run () final {
        uint8_t * local_buf = (uint8_t *)context_->buf;
        *((uint64_t *)local_buf) = 0;
        do {
            usleep(10);// do nothing
        } while(*((uint64_t *)local_buf) != END_FLAG); // the first byte is doorbell byte
        fprintf(stderr, "Detected finish flag at server\n");
        
        return ;
    }
};

class Client1 : public Client {
private:
    std::unique_ptr<RDMAContext> context_;
    std::unique_ptr<Socket> socket_;
    
public:
    Client1(RDMADevice * device, const std::string & ip, const std::string & port) : Client(ip, port) {        
        char * tmp = new char[BUFSIZE];
        auto pair2 = device->open(tmp, BUFSIZE, 20);
        context_ = std::move(pair2.first);
        if(pair2.second != Status::Ok) {
            fprintf(stderr, "open context error\n");
            exit(-1);
        }
    }

    ~Client1() {
        delete [] (char *) context_->buf;
    }

    bool Connect() final {
        socket_ = std::move(Socket::make_socket(CLIENT, stoi(port_), ip_));
        int socket_fd = socket_->GetSockID();
        context_->default_connect(socket_fd); 
        return true;
    }

    void Write(char * buffer) final {
        context_->post_write((uint8_t *) buffer, gLength, 8, 8);
        context_->poll_one_completion(true);
    }

    void Close() final {
        context_->post_write((uint8_t *) &(END_FLAG), 8, 0, 0);
        context_->poll_one_completion(true);
    }
};


// RDMA to PMEM directly
class Server2 : public Server {
private:
    std::unique_ptr<RDMAContext> context_;
    std::unique_ptr<Socket> socket_;
    int dax_fd_;

public:
    Server2(RDMADevice * device, const std::string & port) : Server(port) {
        dax_fd_ = open(PMEM_DEVICE.c_str(), O_RDWR);
        assert(dax_fd_ > 0);
        int id = stoi(port) - RDMA_PORT;
        char * pmemaddr = (char *)mmap(0, BUFSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dax_fd_, id * BUFSIZE);
        if(pmemaddr == NULL || (uint64_t)pmemaddr == 0xffffffffffffffff) {
            fprintf(stderr, "mmap pmem error\n");
            close(dax_fd_);
            exit(-1);
        }

        auto pair2 = device->open(pmemaddr, BUFSIZE, 20);
        context_ = std::move(pair2.first);
        if(pair2.second != Status::Ok) {
            fprintf(stderr, "open context error\n");
            pmem_unmap(pmemaddr, BUFSIZE);
            exit(-1);
        }

        socket_ = std::move(Socket::make_socket(SERVER, stoi(port)));
        int socket_fd = socket_->GetSockID();
        context_->default_connect(socket_fd);
    }

    ~Server2() {
        pmem_unmap(context_->buf, BUFSIZE);
        close(dax_fd_);
    }

    void Run () final {
        uint8_t * local_buf = (uint8_t *)context_->buf;
        *((uint64_t *)local_buf) = 0;
        do {
            usleep(10);// do nothing
        } while(*((uint64_t *)local_buf) != END_FLAG); // the first byte is doorbell byte
        fprintf(stderr, "Detected finish flag at server\n");
        
        return ;
    }
};

class Client2 : public Client {
private:
    std::unique_ptr<RDMAContext> context_;
    std::unique_ptr<Socket> socket_;
    
public:
    Client2(RDMADevice * device, const std::string & ip, const std::string & port) : Client(ip, port) {
        char * tmp = new char[BUFSIZE];
        auto pair2 = device->open(tmp, BUFSIZE, 20);
        context_ = std::move(pair2.first);
        if(pair2.second != Status::Ok) {
            fprintf(stderr, "open context error\n");
            exit(-1);
        }
    }

    ~Client2() {
        delete [] (char *) context_->buf;
    }

    bool Connect() final {
        socket_ = std::move(Socket::make_socket(CLIENT, stoi(port_), ip_));
        int socket_fd = socket_->GetSockID();
        context_->default_connect(socket_fd); 
        return true;
    }

    void Write(char * buffer) final {
        context_->post_write((uint8_t *) buffer, gLength, 8, 8);
        context_->poll_one_completion(true);
    }

    void Close() final {
        context_->post_write((uint8_t *) &(END_FLAG), 8, 0, 0);
        context_->poll_one_completion(true);
    }
};


// RDMA to DRAM then to SSD
class Server3 : public Server {
private:
    std::unique_ptr<RDMAContext> context_;
    std::unique_ptr<Socket> socket_;
    int fd_;

public:
    Server3(RDMADevice * device, const std::string & port) : Server(port) {
        std::string filename = NVME_FILE + port + ".dat";
        fd_ = open(filename.c_str(), O_RDWR | O_CREAT, 0666);
        int ret = ftruncate(fd_, 4UL * 1024 * 1024 * 1024);
        assert(ret == 0);
        lseek(fd_, 0, SEEK_SET);
        if(fd_ <= 0) {
            fprintf(stderr, "open file error\n");
        }

        void * tmp = nullptr;
        ret = posix_memalign(&tmp, 4096, BUFSIZE);
        auto pair2 = device->open(tmp, BUFSIZE, 20);
        context_ = std::move(pair2.first);
        if(pair2.second != Status::Ok) {
            fprintf(stderr, "open context error\n");
            exit(-1);
        }

        socket_ = std::move(Socket::make_socket(SERVER, stoi(port)));
        int socket_fd = socket_->GetSockID();
        context_->default_connect(socket_fd);
    }

    ~Server3() {
        free((void *)context_->buf);
        close(fd_);    
    }

    void Run () final {
        char * local_buf = (char *) context_->buf;
        uint64_t * header = (uint64_t *)context_->buf;
        while(true) {
            while(*header != CLIENT_FLAG && *header != END_FLAG) asm("nop");
            if(*header == END_FLAG) {
                break;
            }
            int ret = write(fd_, local_buf, gLength);
            assert(ret == gLength);
            fdatasync(fd_);
            *header = SERVER_FLAG; // update this for next clerk write
            context_->post_write(nullptr, 8, 0, 0);
            context_->poll_one_completion(true);
        }

        fprintf(stderr, "Detected finish flag at server\n");
        return ;
    }

};

class Client3 : public Client {
private:
    std::unique_ptr<RDMAContext> context_;
    std::unique_ptr<Socket> socket_;
    
public:
    Client3(RDMADevice * device, const std::string & ip, const std::string & port) : Client(ip, port) {
        char * tmp = new char[BUFSIZE];
        auto pair2 = device->open(tmp, BUFSIZE, 20);
        context_ = std::move(pair2.first);
        if(pair2.second != Status::Ok) {
            fprintf(stderr, "open context error\n");
            exit(-1);
        }
    }

    ~Client3() {
        delete [] (char *) context_->buf;
    }

    bool Connect() final {
        socket_ = std::move(Socket::make_socket(CLIENT, stoi(port_), ip_));
        int socket_fd = socket_->GetSockID();
        context_->default_connect(socket_fd); 
        return true;
    }

    void Write(char * buffer) final {
        uint64_t * header = (uint64_t *) context_->buf;
        memcpy((char *)header, buffer, gLength);
        *header = CLIENT_FLAG;
        context_->post_write(nullptr, gLength, 0, 0);
        context_->poll_one_completion(true);

        while(*header != SERVER_FLAG) asm("nop");
    }

    void Close() final {
        uint64_t * header = (uint64_t *) context_->buf;
        *header = END_FLAG;
        context_->post_write(nullptr, gLength, 0, 0);
        context_->poll_one_completion(true);
    }
};


// RDMA to DRAM then to PMR
class Server4 : public Server {
private:
    std::unique_ptr<RDMAContext> context_;
    std::unique_ptr<Socket> socket_;
    char * dmabuf_;

public:
    Server4(RDMADevice * device, const std::string & port) : Server(port) {
        char * tmp = new char[BUFSIZE];
        auto pair2 = device->open(tmp, BUFSIZE, 20);
        context_ = std::move(pair2.first);
        if(pair2.second != Status::Ok) {
            fprintf(stderr, "open context error\n");
            exit(-1);
        }

        #ifdef DMABUF
            int dmabuf_fd = mapcmb(NVME_DEVICE, BUFSIZE);
            if(dmabuf_fd <= 0) {
                fprintf(stderr, "open dmabuf error\n");
                exit(-1);
            }
            context_->register_memory(dmabuf_fd, BUFSIZE);
            dmabuf_ = (char *) mmap(0, BUFSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dmabuf_fd, 0);
        #else
            dmabuf_ = new char[BUFSIZE];
        #endif

        socket_ = std::move(Socket::make_socket(SERVER, stoi(port)));
        int socket_fd = socket_->GetSockID();
        context_->default_connect(socket_fd);
    }

    ~Server4() {
        delete [] (char *)(context_->buf);

        #ifdef DMABUF
            munmap(dmabuf_, BUFSIZE);
        #else
            delete [] dmabuf_;        
        #endif
    }

    void Run () final {
        char * local_buf = (char *) context_->buf;
        uint64_t * header = (uint64_t *)context_->buf;
        while(true) {
            while(*header != CLIENT_FLAG && *header != END_FLAG) asm("nop");
            if(*header == END_FLAG) {
                break;
            }
            memcpy(dmabuf_, local_buf, gLength);
            clflush(dmabuf_, gLength);
            *header = SERVER_FLAG; // update this for next clerk write
            context_->post_write(nullptr, 8, 0, 0);
            context_->poll_one_completion(true);
        }

        fprintf(stderr, "Detected finish flag at server\n");
        return ;
    }
};

class Client4 : public Client {
private:
    std::unique_ptr<RDMAContext> context_;
    std::unique_ptr<Socket> socket_;
    
public:
    Client4(RDMADevice * device, const std::string & ip, const std::string & port) : Client(ip, port) {
        char * tmp = new char[BUFSIZE];
        auto pair2 = device->open(tmp, BUFSIZE, 20);
        context_ = std::move(pair2.first);
        if(pair2.second != Status::Ok) {
            fprintf(stderr, "open context error\n");
            exit(-1);
        }
    }

    ~Client4() {
        delete [] (char *) context_->buf;
    }

    bool Connect() final {
        socket_ = std::move(Socket::make_socket(CLIENT, stoi(port_), ip_));
        int socket_fd = socket_->GetSockID();
        context_->default_connect(socket_fd); 
        return true;
    }

    void Write(char * buffer) final {
        uint64_t * header = (uint64_t *) context_->buf;
        memcpy((char *)header, buffer, gLength);
        *header = CLIENT_FLAG;
        context_->post_write(nullptr, gLength, 0, 0);
        context_->poll_one_completion(true);

        while(*header != SERVER_FLAG) asm("nop");
    }

    void Close() final {
        uint64_t * header = (uint64_t *) context_->buf;
        *header = END_FLAG;
        context_->post_write(nullptr, gLength, 0, 0);
        context_->poll_one_completion(true);
    }
};