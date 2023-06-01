/*
    CopyRight (c) Luo Yongping
*/

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <x86intrin.h>

#ifdef DMABUF
    #include <include/uapi/linux/nvme_ioctl.h>

    inline int mapcmb(std::string device, uint64_t size) {
    int fd, dmabuf_fd;

    struct nvme_alloc_user_cmb opts = {0, 0, 0, size, 0};
    // accquire the dmabuf by nvme ioctl
    fd = open(device.c_str(), O_RDONLY);
    if(fd < 3) {
        perror("open nvme device");
        exit(-1);
    }

    ioctl(fd, NVME_IOCTL_ALLOC_USER_CMB, &opts);
    dmabuf_fd = opts.fd;
    if(dmabuf_fd < 3) {
        perror("get dmabuf fd");
        exit(-1);
    }

    close(fd);
    return dmabuf_fd;
}
#endif

inline bool file_exist(const std::string &filename) {
    struct stat buffer;
    const char * tmp = filename.c_str();
    return (stat(tmp, &buffer) == 0);
}

inline void clflush(void *data, int len, bool fence=true)
{
    volatile char *ptr = (char *)((unsigned long long)data &~(63));
    for(; ptr < (char *)data + len; ptr += 64){
        _mm_clflushopt((void *)ptr);
    }
    asm volatile("sfence" ::: "memory");
}