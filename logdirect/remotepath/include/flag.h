#pragma once

#include <string>

static const std::string RDMA_DEVICE = "mlx5_0";
static const int RDMA_PID = 1;
static const int RDMA_GID  = 3;
static const std::string RDMA_IP = "192.168.2.1";
static const int RDMA_PORT = 9000;
static const int BUFSIZE = 128 * 1024;
static const std::string NVME_DEVICE = "/dev/nvme0";
static const std::string PMEM_DEVICE = "/dev/dax1.0";
static const std::string NVME_FILE = "/data/nvme0/lyp";
static const uint64_t END_FLAG    = 0x5a5a5a5a5a5a5a5a; 
static const uint64_t SERVER_FLAG = 0x7f7f7f7f7f7f7f7f;
static const uint64_t CLIENT_FLAG = 0xf7f7f7f7f7f7f7f7;