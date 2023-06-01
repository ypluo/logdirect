#include <iostream>
#include <omp.h>
#include <unistd.h>
#include <cassert>
#include <vector>
#include <random>
#include <algorithm>
#include <set>

#include "randset.h"
#include "dmabuf.h"
#include "utils.h"
#include "access.h"

using std::cout;
using std::endl;
using std::vector;

double run_test(char * buffer, long size, bool read) {
    drop_cache();
    
    uint32_t count = BUFSIZE / size;
    auto start = seconds();

    if(read) {
        rand_read_lat(buffer, size, count);
    } else {
        rand_write_lat(buffer, size, count);
    }

    auto end = seconds();
    return end - start;
}

void init_memory(char * buffer) {
    RandSet<uint64_t> set;
    for(uint64_t i = 0; i < BUFSIZE / ALIGN_SIZE; i++) {
        set.insert(i * ALIGN_SIZE);
    }

    uint64_t last_pos = 0;
    set.remove(0);
    for(int i = 0; i < (BUFSIZE / ALIGN_SIZE) - 1; i++) {
        uint64_t getone = set.get();
        *(uint64_t *)(buffer + last_pos) = getone;
        set.remove(getone);
        last_pos = getone;
    }
}

void print_help(char * name) {
    cout << "Usage: " << name <<" options" << endl;
    cout << "\t -p: test on DMABUF, if not specified, test on DRAM" << endl;
    cout << "\t -w: write latency, if not specified, test read latency" << endl;
    cout << "\t -r: randomly, if not specified, test sequentially" << endl;
    return ;
}

int main(int argc, char ** argv) {
    bool opt_read = true;
    bool opt_dmabuf = false;
    long opt_size = 64;

    static const char * optstr = "pwhs:";
    char opt;
    while((opt = getopt(argc, argv, optstr)) != -1) {
        switch(opt) {
            case 'p':
                opt_dmabuf = true; 
                break;
            case 'w': 
                opt_read = false; 
                break;
            case 's': 
                opt_size = atoi(optarg); 
                break;
            case 'h':
            case '?': print_help(argv[0]); exit(-1);
            default:  print_help(argv[0]); exit(-1);
        }
    }

    // print the options
    #ifdef DEBUG
        cout << "dmabuf: " << (opt_dmabuf ? 1 : 0) << endl;
        cout << "read: " << (opt_read ? 1 : 0) << endl;
        cout << "size: " << opt_size << endl;
    #endif

    // allocating memory
    char * buf0 = NULL, *buf;
    int importer_fd = 0, pcie_fd = 0;
    if (opt_dmabuf) {
        #ifdef DMABUF
            int dmabuf_fd = mapcmb(PMR_DEVICE, BUFSIZE);
            importer_fd = attach(dmabuf_fd);
            buf0 = (char *)mmap(0, BUFSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dmabuf_fd, 0);
        #else
            pcie_fd = open("/sys/bus/pci/devices/0000:18:00.0/resource4", O_RDWR);
            buf0 = (char *)mmap(0, BUFSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pcie_fd, 0);
        #endif

        if(importer_fd <= 0 && pcie_fd <= 0) {
            printf("open pcie device %d\n", pcie_fd);
            exit(-1);
        }
        buf = buf0;
    } else {
        buf0 = (char *) malloc(BUFSIZE + ALIGN_SIZE);
        uint64_t tmp = (uint64_t)buf0;
        uint64_t remaining = tmp & (ALIGN_SIZE - 1);
        buf = (char *) (remaining == 0 ? tmp : tmp + ALIGN_SIZE - remaining);
    }

    try {
        init_memory(buf);
        double elaspe_time = run_test(buf, opt_size, opt_read);
        printf("%4.1lf\n", elaspe_time / (BUFSIZE / opt_size) * 1000000000);
    } catch(...) {
        std::cout << "error happend" << std::endl;
        // free memory
        if(opt_dmabuf) {
            #ifdef DMABUF
                munmap(buf0, BUFSIZE);
                detach(importer_fd);
            #else
                munmap(buf0, BUFSIZE);
                close(pcie_fd);
            #endif
        } else {
            free(buf0);
        }     
    }

    // free memory
    if(opt_dmabuf) {
        #ifdef DMABUF
            munmap(buf0, BUFSIZE);
            detach(importer_fd);
        #else
            munmap(buf0, BUFSIZE);
            close(pcie_fd);
        #endif
    } else {
        free(buf0);
    }  
    return 0;
}