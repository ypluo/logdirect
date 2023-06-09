/*
    CopyRight (c) Xiong ZiWei, Luo Yongping
*/

#ifndef __RDMA_UTIL__
#define __RDMA_UTIL__

#include <memory>
#include <optional>
#include <functional>
#include <iostream>
#include <cstring>

#include <unistd.h>
#include <sys/mman.h>
#include <infiniband/verbs.h>
#include <byteswap.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

namespace RDMAUtil {
    struct connection_certificate {
        uint64_t addr;   // registered memory address
        uint32_t rkey;   // remote key
        uint32_t qp_num; // local queue pair number
        uint16_t lid;    // LID of the ib port
        uint8_t gid[16]; // mandatory for RoCE

        auto dump() -> void {
            std::cout << "Registered memory: " << addr << "\n";
            std::cout << "remote key: " << rkey << "\n";
            std::cout << "qp_num: " << qp_num << "\n";
            std::cout << "lid: " << lid << "\n";
            std::cout << "gid: ";
            for (int i = 0; i < 16; i++) {
                std::cout << int(gid[i]) << " ";
            }
            std::cout << "\n";
        }
    } __attribute__((packed));

    namespace Constants {
        static constexpr uint32_t MAX_QP_DEPTH = 8;
    }

    namespace Enums {
        enum class Status {
            Ok,
            NoRDMADeviceList,
            DeviceNotFound,
            DeviceNotOpened,
            NoGID,
            CannotOpenDevice,
            CannotAllocPD,
            CannotCreateCQ,
            CannotRegMR,
            CannotCreateQP,
            CannotQueryPort,
            InvalidGIDIdx,
            InvalidIBPort,
            InvalidArguments,
            CannotInitQP,
            QPRTRFailed,
            QPRTSFailed,
            ReadError,
            WriteError,
            PostFailed,
            RecvFailed,
        };
    }
    using namespace Enums;
    using StatusPair = std::pair<Enums::Status, int>;
    auto decode_rdma_status(const Enums::Status& status) -> const char *;

    const int MAX_INLINE_SIZE = 128;
    
    class RDMADevice;
    // Aggregation of pointers to ibv_context, ibv_pd, ibv_cq, ibv_mr and ibv_qp, which are used for further operations
    struct RDMAContext {
        struct ibv_context *ctx;
        struct ibv_pd *pd;
        struct ibv_cq *out_cq;
        struct ibv_cq *in_cq;
        struct ibv_mr *mr;
        struct ibv_mr *tmp_mr;
        struct ibv_qp *qp;
        struct ibv_sge sg;
        struct ibv_send_wr sr;
        connection_certificate local, remote;
        void *buf;
        int bufsize;
        bool dmabuf;
        RDMADevice *device;

        auto post_send_helper(const uint8_t *msg, size_t msg_len, enum ibv_wr_opcode opcode, size_t local_offset,
                              size_t remote_offset) -> StatusPair;
        auto post_send_helper(const uint8_t * &ptr, const uint8_t *msg, size_t msg_len, enum ibv_wr_opcode opcode,
                              size_t local_offset) -> StatusPair;


        RDMAContext() = default;
        RDMAContext(const RDMAContext &) = delete;
        RDMAContext(RDMAContext &&) = delete;
        auto operator=(const RDMAContext &) = delete;
        auto operator=(RDMAContext &&) = delete;

        inline static auto make_rdma_context() -> std::unique_ptr<RDMAContext> {
            auto ret = std::make_unique<RDMAContext>();
            memset(ret.get(), 0, sizeof(RDMAContext));
            return ret;
        }

        inline void register_memory(int dmabuf_fd, int bufsize) {
            tmp_mr = ibv_reg_dmabuf_mr(pd, 0, bufsize, 0, dmabuf_fd, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
        }

        ~RDMAContext() {
            if (qp) ibv_destroy_qp(qp);
            if (mr) ibv_dereg_mr(mr);
            if (tmp_mr) ibv_dereg_mr(tmp_mr);
            if (out_cq) ibv_destroy_cq(out_cq);
            if (in_cq) ibv_destroy_cq(in_cq);
            if (pd) ibv_dealloc_pd(pd);
            if (dmabuf) munmap(buf, bufsize);
            // do not release the ctx because it's shared by multiple RDMAContext instances
        }

        auto default_connect(int socket) -> int;

        auto modify_qp(struct ibv_qp_attr &, int mask) noexcept -> StatusPair;
        auto exchange_certificate(int sockfd) noexcept -> Status;

        auto post_send(const uint8_t *msg, size_t msg_len, size_t local_offset = 0)
            -> StatusPair;
        auto post_send(const uint8_t * &ptr, uint8_t *msg, size_t msg_len,
                       size_t local_offset = 0)
            -> StatusPair;

        auto post_read(size_t msg_len, size_t local_offset = 0, size_t remote_offset = 0)
            -> StatusPair;
        auto post_read(const uint8_t * &ptr, size_t msg_len, size_t local_offset = 0)
            -> StatusPair;

        auto post_write(const uint8_t *msg, size_t msg_len, size_t local_offset = 0,
                        size_t remote_offset = 0)
            -> StatusPair;
        auto post_write(const uint8_t * &ptr, const uint8_t *msg, size_t msg_len,
                        size_t local_offset = 0)
            -> StatusPair;

        auto post_recv_to(size_t msg_len, size_t offset = 0) -> StatusPair;

        auto generate_sge(const uint8_t * msg, size_t msg_len, size_t offset) -> std::unique_ptr<struct ibv_sge>;

        /*
         * Generate a send wr, with send_flag being IBV_SEND_SIGNALED and next being nullptr
         */
        auto generate_send_wr(int wr_id, struct ibv_sge *sge, int num_sge, const uint8_t * remote,
                              struct ibv_send_wr *next = nullptr,
                              enum ibv_wr_opcode opcode = IBV_WR_RDMA_WRITE)
            -> std::unique_ptr<struct ibv_send_wr>;

        auto post_batch_write(struct ibv_send_wr *wrs) -> StatusPair;
        auto post_batch_read(struct ibv_send_wr *wrs) -> StatusPair;
        auto post_batch_write_test() -> void;

        /*
         * A set of poll_completion functions.
         * poll_completion_once(): just to check if a completion is generated
         * poll_one_completion(): poll if one completion is generated and return a ibv_wc struct or an error
         */
        auto poll_completion_once(bool send = true) noexcept -> int;
        auto poll_one_completion(bool send = true) noexcept -> int;
        // auto poll_one_completion(bool send = true) noexcept
        //     -> std::pair<std::unique_ptr<struct ibv_wc>, int>;
        auto poll_multiple_completions(size_t no, bool send = true) noexcept
            -> std::pair<std::unique_ptr<struct ibv_wc[]>, int>;

        // return the address of the start of the write location
        auto fill_buf(const uint8_t *msg, size_t msg_len, size_t offset = 0) -> uint8_t *;

        inline auto get_buf() const noexcept -> const void * {
            return buf;
        }

        inline auto get_byte_buf() const noexcept -> uint8_t * {
            return reinterpret_cast<uint8_t *>(buf);
        }
    };

    /*
     * A wrapping class presenting a single RDMA device, all qps are created from a device should be
     * created by invoking RDMADevice::open()
     */
    class RDMADevice {
    private:
        static const struct ibv_qp_init_attr ZATTR;

        std::string dev_name;
        struct ibv_device **devices;
        struct ibv_device *device;
        struct ibv_context *ctx;
        int ib_port;
        int gid_idx;

    public:
        static auto make_rdma(const std::string &dev_name, int ib_port, int gid_idx)
            -> std::pair<std::unique_ptr<RDMADevice>, Status>
        {
            int dev_num = 0;
            auto ret = std::make_unique<RDMADevice>();
            ret->dev_name = dev_name;
            ret->ib_port = ib_port;
            ret->gid_idx = gid_idx;

            ret->devices = ibv_get_device_list(&dev_num);
            if (!ret->devices) {
                return std::make_pair(nullptr, Status::NoRDMADeviceList);
            }

            for (int i = 0; i < dev_num; i++) {
                if (dev_name.compare(ibv_get_device_name(ret->devices[i])) == 0) {
                    if (auto ctx = ibv_open_device(ret->devices[i]); ctx) {
                        ret->device = ret->devices[i];
                        ret->ctx = ctx;
                        return std::make_pair(std::move(ret), Status::Ok);
                    }
                }
            }

            return std::make_pair(nullptr, Status::DeviceNotFound);
        }

        // never explicitly instantiated
        RDMADevice() :
            dev_name(""),
            devices(nullptr),
            device(nullptr),
            ctx(nullptr),
            ib_port(-1),
            gid_idx(-1) {};
        ~RDMADevice() {
            if (devices) ibv_free_device_list(devices);
            if (ctx) ibv_close_device(ctx);
        }
        RDMADevice(const RDMADevice &) = delete;
        RDMADevice(RDMADevice &&) = delete;
        RDMADevice &operator=(const RDMADevice &) = delete;
        RDMADevice &operator=(RDMADevice&&) = delete;


        inline auto get_ib_port() const noexcept -> const int & {
            return ib_port;
        }

        inline auto get_gid_idx() const noexcept -> const int & {
            return gid_idx;
        }

        inline auto get_dev_name() const noexcept -> const std::string & {
            return dev_name;
        }

        /*
          Open an initialized RDMA device made from `make_rdma`
          @membuf: memory region to be registered
          @memsize: memory region size
          @cqe: completion queue capacity
          @attr: queue pair initialization attribute.
          No need to fill the `send_cq` and `recv_cq` fields, they are filled automatically
        */
        auto open(void *membuf, size_t memsize, size_t cqe, int mr_access = 0,
                  struct ibv_qp_init_attr attr = ZATTR)
            -> std::pair<std::unique_ptr<RDMAContext>, Status>;
        
        /*
          Open an initialized RDMA device made from `make_rdma`
          @dmabuf_fd: file discriptor representing a dma-buf object
          @buflen: the length of the dma-buf
        */
        auto open(int dmabuf_fd, size_t buflen, size_t cqe, int mr_access = 0,
                          struct ibv_qp_init_attr attr = ZATTR)
            -> std::pair<std::unique_ptr<RDMAContext>, Status>;

        inline static auto get_default_mr_access() -> int {
            return IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
        }

        static auto get_default_qp_init_attr() -> struct ibv_qp_init_attr;

        static auto get_default_qp_init_state_attr(const int ib_port = 1)
            -> std::unique_ptr<struct ibv_qp_attr>;
        inline static auto get_default_qp_init_state_attr_mask() -> int {
            return IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
        }

        static auto get_default_qp_rtr_attr(const connection_certificate &remote,
                                            const int ib_port,
                                            const int sgid_idx)
            -> std::unique_ptr<struct ibv_qp_attr>;
        static auto get_default_qp_rtr_attr_mask() -> int {
            return IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
        }

        static auto get_default_qp_rts_attr() -> std::unique_ptr<struct ibv_qp_attr>;
        static auto get_default_qp_rts_attr_mask() -> int {
            return IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
        }
    };
}

#endif // __RDMA_UTIL__