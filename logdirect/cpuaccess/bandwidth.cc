/*
    Copyright (c) Luo Yongping. THIS SOFTWARE COMES WITH NO WARRANTIES, 
    USE AT YOUR OWN RISKS!
*/

#include <iostream>
#include <string>
#include <random>
#include <thread>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cassert>
#include <exception>

#include "dmabuf.h"
#include "utils.h"
#include "access.h"

using std::cout;
using std::endl;
using std::string;

double run_test(char * buf, int test_id, long size, long count, int thread_num) {
    drop_cache(); // clear the LLC cache
    
    auto start = seconds();
    // start all threads
    std::thread test_threads[thread_num];
    for(int i = 0; i < thread_num; i++) {
        if(test_id <= SEQ_WO) { // sequential
            long offset = i * count / thread_num * size;
            test_threads[i] = std::thread(single_thread_test_func[test_id], buf + offset, size, count / thread_num);
        } else { // random 
            test_threads[i] = std::thread(single_thread_test_func[test_id], buf, size, count / thread_num);
        }
    }

    // wait for all threads finishes
    for(int i = 0; i < thread_num; i++) {
        test_threads[i].join();
    }
    auto end = seconds();
    return end - start;
}

void print_help(char * name) {
    cout << "Usage: " << name <<" options" << endl;
    cout << "\t -p: If specified, use dmabuf" << endl;
    cout << "\t -i: Test ID :" << endl;
    cout << "\t               (0:SEQ_RO,  1:SEQ_WO)" << endl;
    cout << "\t               (2:RND_RO,  3:RND_WO)" << endl;
    cout << "\t -t: Thread number, default 1" << endl;
    cout << "\t -g: Request unit, default 64" << endl;
    return ;
}

int main(int argc, char ** argv) {
    // default value for options
    bool opt_dmabuf = false;
    int opt_id = SEQ_WO;
    int opt_threads = 1;
    int opt_granularity = 64;

    // read options from arguments
    static const char * optstr = "g:t:i:ph";
    char opt;
    while((opt = getopt(argc, argv, optstr))!= -1) {
        switch(opt) {
            case 'p': opt_dmabuf = true; break;
            case 'i': opt_id = atoi(optarg); assert(opt_id >= 0 && opt_id <= 3); break;
            case 't': opt_threads = atoi(optarg); break;
            case 'g': opt_granularity = (atoi(optarg) / 64) * 64; break;
            case 'h': 
            case '?': print_help(argv[0]); exit(-1);
            default : print_help(argv[0]); exit(-1);
        }
    }

    // print the options
    #ifdef DEBUG
        cout << "workload type: " << TEST_NAME[opt_id] << endl;
        cout << "thread number: " << opt_threads << endl;
        cout << "request unit : " << opt_granularity << "B" << endl;
        if(opt_dmabuf) {
            cout << "test environment: Optane" << endl;
        } else {
            cout << "test environment: DRAM" << endl;
        }
    #endif

    // preparing
    char * buf0 = NULL, * buf;
    int importer_fd = 0, pcie_fd = 0;
    if (opt_dmabuf) {
        assert(opt_threads < 2 || opt_id >= RND_RO); // dmabuf do not support this bandwidth test configure
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
        buf = buf0; // mmap address must be ALIGNED
    } else {
        buf0 = (char *) malloc(BUFSIZE + 1024 * 1024);
        uint64_t tmp = (uint64_t)buf0;
        uint64_t remaining = tmp & (ALIGN_SIZE - 1);
        buf = (char *) (remaining == 0 ? tmp : tmp + ALIGN_SIZE - remaining);
    }

    try {
        // the buffer address is already aligned 
        double elaspe_time = run_test(buf, opt_id, opt_granularity, BUFSIZE / opt_granularity, opt_threads);
        // output the result throughput
        cout << int(BUFSIZE / (1024 * 1024) / elaspe_time) << endl;
    } catch(...) {
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
