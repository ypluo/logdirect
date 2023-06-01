#include "client.h"
#include "server.h"

namespace frontend {
SyncClient::SyncClient(MyOption opt, int id) {
    client_id_ = id;
    ip_ = opt.ipaddr;
    port_ = opt.ipport;

    auto device = RDMADevice::make_rdma(opt.rdma_device, opt.port, opt.gid);
    assert(device != nullptr);
    rdma_device_ = std::move(device);

    auto [context, status] = rdma_device_->open();
    if(status != Status::Ok) {
        fprintf(stderr, "%s\n", decode_rdma_status(status).c_str());
    }
    local_buf_ = new uint8_t[MAX_REQUEST + RING_HEADER];
    context->register_write_buf(local_buf_, MAX_REQUEST + RING_HEADER);
    rdma_context_ = std::move(context);
}

void SyncClient::Connect() {
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
    // init the header
    uint32_t * header = (uint32_t *) local_buf_;
    *header = CLIENT_DONE;
}

void SyncClient::SendWrite(const char * key, const char * val, Operation op) {
    uint16_t key_len = strlen(key);
    uint16_t val_len = strlen(val);
    uint16_t total_len = sizeof(Request) + key_len + val_len;

    // prepare the record in buffer[RING_HEADER:]
    Request * request = (Request *)(local_buf_ + RING_HEADER);
    request->op = op;
    request->key_size = key_len;
    request->val_size = val_len;
    memcpy(local_buf_ + RING_HEADER + sizeof(Request), key, key_len);
    memcpy(local_buf_ + RING_HEADER + sizeof(Request) + key_len, val, val_len);

    // write the record to the remote: use buffer[RING_HEADER:]
    rdma_context_->post_write(nullptr, total_len, RING_HEADER, RING_HEADER, false);
    rdma_context_->post_write(nullptr, sizeof(uint32_t), 0, 0, true);
    rdma_context_->poll_one_completion(true);
    
    // wait for the clerk side to update this field
    uint32_t * header = (uint32_t *) local_buf_;
    while(*header != CLERK_DONE) asm("nop");
    *header = CLIENT_DONE; // update this for next client write

    return ;
}

bool SyncClient::SendGet(const char * key, std::string * val) {
    uint16_t key_len = strlen(key);
    uint16_t total_len = sizeof(Request) + key_len;

    // prepare the record in buffer[RING_HEADER:]
    Request * request = (Request *)(local_buf_ + RING_HEADER);
    request->op = GET;
    request->key_size = strlen(key);
    request->val_size = 0;
    memcpy(local_buf_ + RING_HEADER + sizeof(Request), key, strlen(key));

    // write the record to the remote: use buffer[RING_HEADER:]
    rdma_context_->post_write(nullptr, total_len, RING_HEADER, RING_HEADER, false);
    rdma_context_->post_write(nullptr, sizeof(uint32_t), 0, 0, true);
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

void SyncClient::SendSync() {
    // prepare the record in buffer[RING_HEADER:]
    Request * request = (Request *)(local_buf_ + RING_HEADER);
    request->op = SYNC;
    request->key_size = 0;
    request->val_size = 0;

    // write the record to the remote: use buffer[RING_HEADER:]
    rdma_context_->post_write(nullptr, sizeof(Request), RING_HEADER, RING_HEADER, false);
    rdma_context_->post_write(nullptr, sizeof(uint32_t), 0, 0, true);
    rdma_context_->poll_one_completion(true);

    // wait for the clerk side to update this field
    uint32_t * header = (uint32_t *) local_buf_;
    while(*header != CLERK_DONE) asm("nop");
    *header = CLIENT_DONE; // update this for next client write
}

bool SyncClient::SendDelete(const char * key) {
    uint16_t key_len = strlen(key);
    uint16_t total_len = sizeof(Request) + key_len;

    // prepare the record in buffer[RING_HEADER:]
    Request * request = (Request *)(local_buf_ + RING_HEADER);
    request->op = DELETE;
    request->key_size = strlen(key);
    request->val_size = 0;
    memcpy(local_buf_ + RING_HEADER + sizeof(Request), key, strlen(key));

    // write the record to the remote: use buffer[RING_HEADER:]
    rdma_context_->post_write(nullptr, total_len, RING_HEADER, RING_HEADER, false);
    rdma_context_->post_write(nullptr, sizeof(uint32_t), 0, 0, true);
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

void SyncClient::SendClose() {
    Request * request = (Request *)(local_buf_ + RING_HEADER);
    uint16_t total_len = sizeof(Request);
    request->op = CLOSE;
    request->key_size = 0;
    request->val_size = 0;

    // write the record to the remote: use buffer[RING_HEADER:]
    rdma_context_->post_write(nullptr, total_len, RING_HEADER, RING_HEADER, false);
    rdma_context_->post_write(nullptr, sizeof(uint32_t), 0, 0, true);
    rdma_context_->poll_one_completion(true);

    // send out 
    rdma_context_->post_send(nullptr, MAX_REQUEST, 0);
    rdma_context_->poll_one_completion(true);
}


SyncClerk::SyncClerk(std::unique_ptr<RDMAContext> ctx, DBType * db, int id) {
    context_ = std::move(ctx);
    clerk_id_ = id;
    db_ = db;

    local_buf_ = (uint8_t *)context_->get_write_buf();
}

void SyncClerk::Run(std::unique_ptr<SyncClerk> clk, std::unique_ptr<uint8_t[]> buf) {
    while(true) {
        // wait for the clerk side to update this field
        uint32_t * header = (uint32_t *) clk->local_buf_;
        while(*header != CLIENT_DONE) asm("nop");
        *header = CLERK_DONE; // update this for next clerk write

        Request * request = (Request *)(clk->local_buf_ + RING_HEADER);
        RequestReply * reply = (RequestReply *)(clk->local_buf_ + RING_HEADER);
        uint32_t key_size = request->key_size;
        switch(request->op) {
            case UPDATE: // intended passdown
            case PUT: {
                std::string key((char *)request + sizeof(Request), key_size);
                std::string val((char *)request + sizeof(Request) + key_size, request->val_size);
                if(clk->db_->Put(key, val)) {
                    reply->status = RequestStatus::OK;
                } else {
                    reply->status = RequestStatus::ERROR;
                }
                reply->val_size = 0;
                break;
            }
            case GET: {
                std::string key((char *)request + sizeof(Request), key_size);
                std::string value;
                if(clk->db_->Get(key, &value)) {
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
                clk->context_->post_recv(MAX_REQUEST, 0);
                clk->context_->poll_one_completion(false);
                return ;
            }
            default: {
                fprintf(stderr, "Error operation at Clerk %d\n", clk->clerk_id_);
                exit(-1);
            }
        }

        // write the request reply to client
        clk->context_->post_write(nullptr, sizeof(RequestReply) + reply->val_size, RING_HEADER, RING_HEADER, false);
        // write the clerk done messgae to client
        clk->context_->post_write(nullptr, sizeof(uint32_t), 0, 0, true); // write to client write_buf[0:4]
        clk->context_->poll_one_completion(true);
    }
}


SyncServer::SyncServer(MyOption opt, DBType * db) {
    port_ = opt.ipport;
    db_ = db;

    auto device = RDMADevice::make_rdma(opt.rdma_device, opt.port, opt.gid);
    assert(device != nullptr);
    rdma_device_ = std::move(device); 
}

void SyncServer::Listen() {
    // wait for the first client to connect
    auto socket = Socket::make_socket(SERVER, port_);
    int commu_fd = socket->GetFirst();
    
    // a infinite loop waiting for new connection
    do {
        // create a rdma context
        auto [context, status] = rdma_device_->open();
        if(status != Status::Ok) {
            fprintf(stderr, "%s\n", decode_rdma_status(status).c_str());
        }
        std::unique_ptr<uint8_t[]> mem;
        mem.reset(new uint8_t[MAX_REQUEST + RING_HEADER]);
        context->register_write_buf(mem.get(), MAX_REQUEST + RING_HEADER);

        // exchange rdma context
        if(context->default_connect(commu_fd) == -1) {
            exit(-1);
        }
        
        // create a new thread to accept client request
        std::unique_ptr<SyncClerk> new_clerk = std::make_unique<SyncClerk>(std::move(context), db_, clerk_num_++);
        std::thread th(&(SyncClerk::Run), std::move(new_clerk), std::move(mem));
        th.detach();
        // fprintf(stderr, "Make a clerk serving...\n");

        // wait for a client to connect
        commu_fd = socket->NewChannel(port_);
    } while (true);
}

} // namespace frontend