/*
    Copyright (c) Luo Yongping. THIS SOFTWARE COMES WITH NO WARRANTIES, 
    USE AT YOUR OWN RISKS!
*/

#ifndef __BW_TEST__
#define __BW_TEST__
#include <string>

void seq_read(char * start_addr, long size, long count);
void seq_write(char * start_addr, long size, long count);

void rand_read(char * start_addr, long size, long count);
void rand_write(char * start_addr, long size, long count);

typedef void (*test_func_t)(char * start_addr, long size, long count);

const test_func_t single_thread_test_func [] = {
    // sequential read and write
    &seq_read,
    &seq_write,
    // random read and write
    &rand_read,
    &rand_write,
};

enum {
	SEQ_RO = 0, 
	SEQ_WO = 1, 
	RND_RO = 2, 
	RND_WO = 3
} TEST_ID;

const std::string TEST_NAME [] = {"SEQ_RO", "SEQ_WO","RND_RO", "RND_WO"};

#define RandLFSR64 "mov    (%[random]), %%r9 \n"  \
				   "mov    %%r9, %%r12 \n"        \
				   "shr    %%r9 \n"               \
				   "and    $0x1, %%r12d \n"       \
				   "neg    %%r12 \n"              \
				   "and    %%rcx, %%r12 \n"       \
				   "xor    %%r9, %%r12 \n"        \
				   "mov    %%r12, (%[random]) \n" \
				   "mov    %%r12, %%r8 \n"        \
				   "and    %[accessmask], %%r8 \n"
				   
// load 64 bytes 
#define LD_AVX512	"vmovntdqa 0x0(%%r9, %%r10), %%zmm0 \n add $0x40, %%r10 \n"
					// "mov 0x0(%%r9, %%r10), %%r8 \n add $0x40, %%r10 \n"

#ifdef CLWB
#define WR_AVX512	\
				"vmovdqa64  %%zmm0,  0x0(%%r9, %%r10) \n" \
				"clwb  0x0(%%r9, %%r10) \n" \
				"add $0x40, %%r10 \n"

#elif defined(CLFLUSHOPT)
#define WR_AVX512	\
				"vmovdqa64  %%zmm0,  0x0(%%r9, %%r10) \n" \
				"clflushopt  0x0(%%r9, %%r10) \n" \
				"add $0x40, %%r10 \n"
#else 
#define WR_AVX512	"vmovdqa64  %%zmm0,  0x0(%%r9, %%r10) \n clflush  0x0(%%r9, %%r10) \n add $0x40, %%r10 \n"
				// "mov  %%r8, 0x0(%%r9, %%r10) \n clflush  0x0(%%r9, %%r10) \n add $0x40, %%r10 \n"

#endif

#define BW_FENCE	""

#ifdef CLWB
#define WR_AVX512_2	\
				"vmovdqa64  %%zmm0,  0x0(%%r9, %%r10) \n" \
				"clwb  0x0(%%r9, %%r10) \n"

#elif defined(CLFLUSHOPT)
#define WR_AVX512_2	\
				"vmovdqa64  %%zmm0,  0x0(%%r9, %%r10) \n" \
				"clflushopt  0x0(%%r9, %%r10) \n"
#else 
#define WR_AVX512_2	\
				"vmovdqa64  %%zmm0,  0x0(%%r9, %%r10) \n" \
				"clflush  0x0(%%r9, %%r10) \n"
#endif

#define LAT_FENCE "mfence \n"
// #define LAT_FENCE "\n"

void rand_write_lat(char * start_addr, long size, long count);
void rand_read_lat(char * start_addr, long size, long count);

#endif