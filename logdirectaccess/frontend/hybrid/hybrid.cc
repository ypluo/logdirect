#include "client.h"
#include "server.h"

#ifdef DMABUF
#include "dmabuf.h"
#endif

#define COPY2DRAM
#define BG_SYNC

namespace frontend {

HybridClient::HybridClient(MyOption opt, int id) {
    client_id_ = id;
    ip_ = opt.ipaddr;
    port_ = opt.ipport;
    buf_head_ = RING_HEADER;

    auto device = RDMADevice::make_rdma(opt.rdma_device, opt.port, opt.gid);
    assert(device != nullptr);
    rdma_device_ = std::move(device);
    

    auto [context, status] = rdma_device_->open();
    if(status != Status::Ok) {
        fprintf(stderr, "%s\n", decode_rdma_status(status).c_str());
    }
    local_buf_ = new uint8_t[MAX_REQUEST];
    context->register_write_buf(local_buf_, MAX_REQUEST);
    rdma_context_ = std::move(context);
}

void HybridClient::Connect() {
    // connect the rdma channel to a client rdma channel
    auto socket = Socket::make_socket(CLIENT, port_, ip_);
    int commu_fd = socket->GetFirst();
    // exchange rdma context
    if(rdma_context_->default_connect(commu_fd) == -1) {
        exit(-1);
    }
    // fprintf(stderr, "connect to server\n");
    local_buf_ = (uint8_t *)rdma_context_->get_write_buf();
    assert(local_buf_ != nullptr);
    // init the header_
    header_ = (uint32_t *) local_buf_;
    *header_ = CLIENT_DONE;
    buf_tail_ = (uint32_t *) (local_buf_ + sizeof(uint32_t));
    *buf_tail_ = RING_HEADER;
}

void HybridClient::SendWrite(const char * key, const char * val, Operation op) {
    uint16_t key_len = strlen(key);
    uint16_t val_len = strlen(val);
    uint16_t meta_len = sizeof(Request) + key_len;
    uint16_t total_len = sizeof(Request) + key_len + val_len;

    // prepare the record in buffer[RING_HEADER:]
    Request * request = (Request *)(local_buf_ + RING_HEADER);
    if(buf_head_ + total_len >= MAX_ASYNC_SIZE) {
        // the remaining ring buffer is not enough, we should rewind to the back
        // post an empty request as an signal to the end of this run
        request->op = NOP;
        rdma_context_->post_write0(nullptr, sizeof(Request), RING_HEADER, RING_HEADER, false);
        rdma_context_->post_write0(nullptr, sizeof(uint32_t), 0, 0, true);
        rdma_context_->poll_one_completion(true);
        // wait for the clerk side to update this field
        while(*header_ != CLERK_DONE) asm("nop");
        *header_ = CLIENT_DONE;

        // CORRDINATION: if buf_head < buf_tail, we should wait for the tail to rewind first
        while(buf_head_ <= *buf_tail_) {
            usleep(10);
        }
        buf_head_ = RING_HEADER; // rewind to the begining
    } else {
        // CORRDINATION: if the buf_head < buf_tail, but one more request will fill the ring buffer, 
        // we should wait for the buf_tail to move forward. 
        while(buf_head_ < *buf_tail_ && buf_head_ + total_len >= *buf_tail_) { // this request will overlap with the tail
            usleep(10);
        }
    }

    request->op = op;
    request->key_size = key_len;
    request->val_size = val_len;
    memcpy(local_buf_ + RING_HEADER + sizeof(Request), key, key_len);
    memcpy(local_buf_ + RING_HEADER + sizeof(Request) + key_len, val, val_len);

    // write meta data to clerk's send buffer
    rdma_context_->post_write0(nullptr, meta_len, RING_HEADER, RING_HEADER, false);
    // write record data to clerk's write buffer
    rdma_context_->post_write(nullptr, total_len, RING_HEADER, buf_head_, false);
    // write completed: signaled
    rdma_context_->post_write0(nullptr, sizeof(uint32_t), 0, 0, true);
    rdma_context_->poll_one_completion(true);
    buf_head_ += total_len;
    
    // wait for the clerk side to update this field
    while(*header_ != CLERK_DONE) asm("nop");
    *header_ = CLIENT_DONE; // update this for next client write

    return ;
}

bool HybridClient::SendGet(const char * key, std::string * val) {
    uint16_t key_len = strlen(key);
    uint16_t total_len = sizeof(Request) + key_len;

    // prepare the record in buffer[RING_HEADER:]
    Request * request = (Request *)(local_buf_ + RING_HEADER);
    request->op = GET;
    request->key_size = strlen(key);
    request->val_size = 0;
    memcpy(local_buf_ + RING_HEADER + sizeof(Request), key, strlen(key));

    rdma_context_->post_write0(nullptr, total_len, RING_HEADER, RING_HEADER, false);
    // no need to write records to async write buffer
    rdma_context_->post_write0(nullptr, sizeof(uint32_t), 0, 0, true);
    rdma_context_->poll_one_completion(true);

    // wait for the clerk side to update this field
    uint32_t * header = (uint32_t *) local_buf_;
    while(*header != CLERK_DONE) asm("nop");
    *header = CLIENT_DONE; // update this for next client write

    RequestReply * reply = (RequestReply *)(local_buf_ + RING_HEADER);
    if(reply->status == RequestStatus::OK) {
        *val = std::move(std::string(reply->value, reply->val_size));
        return true;
    } else {
        return false;
    }
}

void HybridClient::SendSync() {
    // prepare the record in buffer[RING_HEADER:]
    Request * request = (Request *)(local_buf_ + RING_HEADER);
    request->op = SYNC;
    request->key_size = 0;
    request->val_size = 0;

    // write the record to the remote: use buffer[RING_HEADER:]
    rdma_context_->post_write0(nullptr, sizeof(Request), RING_HEADER, RING_HEADER, false);
    rdma_context_->post_write0(nullptr, sizeof(uint32_t), 0, 0, true);
    rdma_context_->poll_one_completion(true);

    // wait for the clerk side to update this field
    uint32_t * header = (uint32_t *) local_buf_;
    while(*header != CLERK_DONE) asm("nop");
    *header = CLIENT_DONE; // update this for next client write
}

bool HybridClient::SendDelete(const char * key) {
    uint16_t key_len = strlen(key);
    uint16_t total_len = sizeof(Request) + key_len;

    // prepare the record in buffer[RING_HEADER:]
    Request * request = (Request *)(local_buf_ + RING_HEADER);
    request->op = DELETE;
    request->key_size = strlen(key);
    request->val_size = 0;
    memcpy(local_buf_ + RING_HEADER + sizeof(Request), key, strlen(key));

    rdma_context_->post_write0(nullptr, total_len, RING_HEADER, RING_HEADER, false);
    // no need to write records to async write buffer
    rdma_context_->post_write0(nullptr, sizeof(uint32_t), 0, 0, true);
    rdma_context_->poll_one_completion(true);

    // wait for the clerk side to update this field
    uint32_t * header = (uint32_t *) local_buf_;
    while(*header != CLERK_DONE) asm("nop");
    *header = CLIENT_DONE; // update this for next client write

    RequestReply * reply = (RequestReply *)(local_buf_ + RING_HEADER);
    if(reply->status == RequestStatus::OK) {
        return true;
    } else {
        return false;
    }
}

void HybridClient::SendClose() {
    Request * request = (Request *)(local_buf_ + RING_HEADER);
    uint16_t total_len = sizeof(Request);
    request->op = CLOSE;
    request->key_size = 0;
    request->val_size = 0;

    // write close request to clerk's send buffer
    rdma_context_->post_write0(nullptr, total_len, RING_HEADER, RING_HEADER, false);
    rdma_context_->post_write0(nullptr, sizeof(uint32_t), 0, 0, true);
    rdma_context_->poll_one_completion(true);

    // send out 
    rdma_context_->post_send(nullptr, MAX_REQUEST, 0);
    rdma_context_->poll_one_completion(true);
}


HybridClerk::HybridClerk(std::unique_ptr<RDMAContext> ctx, DBType * db, HashType * index, int log_fd, int id) {
    context_ = std::move(ctx);
    clerk_id_ = id;
    db_ = db;
    buf_tail_ = RING_HEADER;
    log_fd_ = log_fd;
    im_index_ = index;

    send_buf_ = (uint8_t *)context_->get_send_buf();
    write_buf_ = (uint8_t *)context_->get_write_buf();
    *((uint32_t *) write_buf_) = RING_HEADER; // buf_head for client to update
}

void HybridClerk::Run(std::unique_ptr<HybridClerk> clk, std::function<void(void)> callback) {
    std::vector<std::string> key_batch;
    std::vector<DBIndex> index_batch;
    uint32_t batch_length = 0;
    auto persist_func = [&clk](uint8_t * buffer, size_t len, uint32_t client_buf_tail) {
        std::lock_guard<std::mutex> lock(clk->mu_);
        // write the batch to log file persistently
        size_t ret = write(clk->log_fd_, buffer, len);
        fdatasync(clk->log_fd_);

        // write to client write_buf[4:8]
        clk->context_->post_write((uint8_t *)(&client_buf_tail), sizeof(uint32_t), 0, sizeof(uint32_t), true);
        clk->context_->poll_one_completion(true);
        #ifdef COPY2DRAM
            delete [] buffer;
        #endif
        return ;
    };

    while(true) {
        // wait for the clerk side to update this field
        uint32_t * header = (uint32_t *) clk->send_buf_;
        while(*header != CLIENT_DONE) asm("nop");
        *header = CLERK_DONE; // update this for next clerk write

        // read meta data from message buffer
        Request * request = (Request *)(clk->send_buf_ + RING_HEADER);
        RequestReply * reply = (RequestReply *)(clk->send_buf_ + RING_HEADER);
        uint32_t key_size = request->key_size;
        switch(request->op) {
            case NOP :    // intended passdown
            case UPDATE : // intended passdown
            case PUT: {
                if(request->op != NOP) {
                    key_batch.emplace_back((char *)request + sizeof(Request), key_size);
                    index_batch.emplace_back(0, batch_length + sizeof(Request), key_size, request->val_size);
                    DBIndex mem_idx(clk->write_buf_ + clk->buf_tail_ + batch_length + sizeof(Request), 
                                    key_size, request->val_size);
                    clk->im_index_->insert_or_assign(key_batch.back(), mem_idx);
                    batch_length += request->Length();
                }
                
                if(batch_length >= BATCH_SIZE || clk->buf_tail_ + batch_length == MAX_ASYNC_SIZE 
                            || request->op == NOP) {
                    uint8_t * start_buf = clk->write_buf_ + clk->buf_tail_;
                    #ifndef COPY2DRAM
                        uint8_t * tmp_buf = start_buf;
                    #else
                        uint8_t * tmp_buf = new uint8_t[batch_length];
                        memcpy(tmp_buf, start_buf, batch_length);
                    #endif
                    clk->db_->PutBatch(key_batch, index_batch, tmp_buf, batch_length);
                    for(int i = 0; i < key_batch.size(); i++) {
                        clk->im_index_->erase(key_batch[i]);
                    }

                    clk->buf_tail_ += batch_length;
                    if(batch_length < BATCH_SIZE || clk->buf_tail_ == MAX_ASYNC_SIZE)
                        clk->buf_tail_ = RING_HEADER;

                    #ifdef BG_SYNC
                        std::thread t(persist_func, tmp_buf, batch_length, clk->buf_tail_);
                        t.detach(); // persist the log asynchronously
                    #else
                        persist_func(tmp_buf, batch_length, clk->buf_tail_);
                    #endif
                    
                    // initate a new batch
                    key_batch.resize(0);
                    index_batch.resize(0);
                    batch_length = 0;
                }

                reply->status = RequestStatus::OK;
                reply->val_size = 0;
                break;
            }
            case GET: {
                std::string key((char *)request + sizeof(Request), key_size);
                std::string value;
                DBIndex mem_idx;
                if(clk->im_index_->find(key, mem_idx)) {
                    reply->status = RequestStatus::OK;
                    reply->val_size = mem_idx.value_size_;
                    memcpy(reply->value, (char *)mem_idx.memaddr_ + key_size, reply->val_size);
                } else if(clk->db_->Get(key, &value)) {
                    reply->status = RequestStatus::OK;
                    reply->val_size = value.size();
                    memcpy(reply->value, value.c_str(), reply->val_size);
                } else {
                    reply->status = RequestStatus::NOTFOUND;
                    reply->val_size = 0;
                }
                break;
            }
            case SYNC: {
                clk->db_->SetSync();
                break;
            }
            case DELETE: {
                std::string key((char *)request + sizeof(Request), key_size);
                std::string value;
                if(clk->db_->Delete(key)) {
                    reply->status = RequestStatus::OK;
                } else {
                    reply->status = RequestStatus::NOTFOUND;
                }
                reply->val_size = 0;
                break;
            }
            case CLOSE: {
                if(batch_length > 0) {
                    uint8_t * start_buf = clk->write_buf_ + clk->buf_tail_;
                    #ifndef COPY2DRAM
                        uint8_t * tmp_buf = start_buf;
                    #else
                        uint8_t * tmp_buf = new uint8_t[batch_length];
                        memcpy(tmp_buf, start_buf, batch_length);
                    #endif
                    clk->db_->PutBatch(key_batch, index_batch, tmp_buf, batch_length);

                    clk->buf_tail_ += batch_length;
                    if(batch_length < BATCH_SIZE || clk->buf_tail_ == MAX_ASYNC_SIZE)
                        clk->buf_tail_ = RING_HEADER;

                    persist_func(tmp_buf, batch_length, clk->buf_tail_);
                }
                // send close msg to client
                clk->context_->post_recv(MAX_REQUEST, 0);
                clk->context_->poll_one_completion(false);

                callback();
                return ;
            }
            default: {
                fprintf(stderr, "Error operation at Clerk %d\n", clk->clerk_id_);
                exit(-1);
            }
        }

        // write the request reply to client
        clk->context_->post_write1(nullptr, sizeof(RequestReply) + reply->val_size, 
                    RING_HEADER, RING_HEADER, false);

        // write the clerk done messgae to client
        clk->context_->post_write1(nullptr, sizeof(uint32_t), 0, 0, true); // write to client write_buf[0:4]
        clk->context_->poll_one_completion(true);
    }
}


HybridServer::HybridServer(MyOption opt, DBType * db) {
    port_ = opt.ipport;
    db_ = db;
    path_ = opt.dir + "/" + opt.db_type + "/" + "pmrlog.log";

    auto device = RDMADevice::make_rdma(opt.rdma_device, opt.port, opt.gid);
    assert(device != nullptr);
    rdma_device_ = std::move(device); 

    #ifdef DMABUF
        dmabuf_fd_ = mapcmb(opt.cmb_device, MAX_DMABUF_SIZE);
        bitmap_ = 0;
        fprintf(stderr, "HybridServer using DMABUF is ON\n");
    #endif
}

void HybridServer::Listen() {
    // wait for the first client to connect
    auto socket = Socket::make_socket(SERVER, port_);
    int commu_fd = socket->GetFirst();
    int log_fd = open(path_.c_str(), O_CREAT | O_WRONLY, 0645);
    assert(ftruncate(log_fd, 8UL * 1024 * 1024 * 1024) == 0);
    lseek(log_fd , 0 , SEEK_SET);
    
    // a infinite loop waiting for new connection
    do {
        // create a rdma context
        auto [context, status] = rdma_device_->open();
        if(status != Status::Ok) {
            fprintf(stderr, "%s\n", decode_rdma_status(status).c_str());
        }

        std::function<void(void)> clear_memory;
        #ifdef DMABUF
        uint8_t slot = __builtin_clzll(~bitmap_);
        if(slot < (MAX_DMABUF_SIZE / MAX_ASYNC_SIZE)) {
            bitmap_ = bitmap_ + ((uint64_t)1 << (63 - slot));
            context->register_write_buf(dmabuf_fd_, MAX_ASYNC_SIZE * (uint64_t)slot, MAX_DMABUF_SIZE);
            clear_memory = [this, slot]() {
                // clear the memory buffer
                this->bitmap_ = this->bitmap_ - ((uint64_t)1 << (63 - slot));
            };
        } else {
        #endif
            uint8_t * buffer = new uint8_t[MAX_ASYNC_SIZE];
            context->register_write_buf(buffer, MAX_ASYNC_SIZE);
            clear_memory = [buffer](void) {
                // clear the memory buffer
                delete [] buffer;
            };
        #ifdef DMABUF
        }
        #endif

        // exchange rdma context
        if(context->default_connect(commu_fd) == -1) {
            exit(-1);
        }
        
        // create a new thread to accept client request
        std::unique_ptr<HybridClerk> new_clerk = std::make_unique<HybridClerk>(std::move(context), db_, &map_, log_fd, clerk_num_++);
        std::thread th(&(HybridClerk::Run), std::move(new_clerk), clear_memory);
        th.detach();
        // fprintf(stderr, "Make a clerk serving...\n");

        // wait for a client to connect
        commu_fd = socket->NewChannel(port_);
    } while (true);
}

} // namespace frontend