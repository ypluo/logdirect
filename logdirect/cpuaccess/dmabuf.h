#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <include/uapi/linux/nvme_ioctl.h>
#include <linux/dma-buf.h>

#define IMPORTER_IOCTL_ATTACH_CMB _IOR('N', 0x60, int)
#define IMPORTER_IOCTL_DETACH_CMB _IO('N', 0x61)

static const std::string PMR_DEVICE = "/dev/nvme0"; // nvme device name

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
    // fprintf(stderr, "get dmabuf success\n");

    close(fd);
    return dmabuf_fd;
}

inline int attach(int dmabuf_fd) {
    int importer_fd;
    int ret;

    /* IMPORTER: importer driver, load by "insmod importer/importer.ko" */
    importer_fd = open("/dev/importer", O_RDWR);
    if (importer_fd < 0) {
        perror("open /dev/importer");
        goto failed_import;
    }
    // fprintf(stderr, "open /dev/importer success\n");

    ret = ioctl(importer_fd, IMPORTER_IOCTL_ATTACH_CMB, &dmabuf_fd);
    if (ret < 0) {
        perror("ioctl attach cmb error");
        goto failed_attach;
    }
    // fprintf(stderr, "attach success\n");

    return importer_fd;

    failed_attach:
    close(importer_fd);

    failed_import:
    exit(-1);
}

inline void detach(int importer_fd) {
    ioctl(importer_fd, IMPORTER_IOCTL_DETACH_CMB, NULL);
    close(importer_fd);
    // fprintf(stderr, "detach importer\n");
    return ;
}