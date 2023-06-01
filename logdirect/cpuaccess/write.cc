#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>

#include "dmabuf.h"

#define IMPORTER_IOCTL_ATTACH_CMB _IOR('N', 0x60, int)
#define IMPORTER_IOCTL_DETACH_CMB _IO('N', 0x61)

int main(void) {
    int fd, dmabuf_fd, importer_fd;
    char * vaddr;
    int ret;
    struct dma_buf_sync sync;

    /* EXPORTER: nvme driver */
    const int access_size = 1 * 4096;
    dmabuf_fd = mapcmb(PMR_DEVICE, access_size);

    /* IMPORTER: importer driver, load by "insmod importer/importer.ko" */
    importer_fd = open("/dev/importer", O_RDWR);
    if (importer_fd < 0) {
        perror("open /dev/importer:");
        goto failed_import;
    }
    printf("open importer success\n");

    ret = ioctl(importer_fd, IMPORTER_IOCTL_ATTACH_CMB, &dmabuf_fd);
    if (ret < 0) {
        printf("ioctl attach cmb error: %d\n", errno);
        goto failed_attach;
    }
    printf("attach success\n");

    /* USERSPACE: mmap and write test */ 
    sync = { 0 };
    sync.flags = DMA_BUF_SYNC_RW | DMA_BUF_SYNC_START;
    ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync);
    // mmap the cmb to userspace
    vaddr = (char *)mmap(0, access_size, PROT_READ | PROT_WRITE, MAP_SHARED, dmabuf_fd, 0);
    if(vaddr == nullptr){
        perror("mmap:");
        goto failed_mmap;
    }

    printf("mmap address %lx\n", (uint64_t) vaddr);

    // write to the dma buffer
    memcpy(vaddr, "123456789012345678901234567890.\0", 32);
    
    // use pwrite to flush contents to SSD directly
    fd = open("/data/nvme2/test.txt", O_RDWR| O_CREAT, 0755);
    // fd = open("/data/nvme2/test.txt", O_RDWR| O_CREAT | O_DIRECT, 0755); // NOT SUPPORTED
    if(fd < 3) {
        perror("open file: ");
    } else {
        int res = write(fd, vaddr, access_size);
        if(res < 0)
            perror("write");
        close(fd);
    }

    // unmap the dma buffer
    munmap(vaddr, access_size);

failed_mmap:
    // close the cpu access
    sync.flags = DMA_BUF_SYNC_RW | DMA_BUF_SYNC_END;
    ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync);
    // detach the CMB dmabuf
    ioctl(importer_fd, IMPORTER_IOCTL_DETACH_CMB, NULL);

failed_attach:
    close(importer_fd);

failed_import:
    return 0;
}