/*
	SPDX-License-Identifier: GPL-2.0
	Modified from src/kernel/memaccess.c in https://github.com/NVSL/OptaneStudy
*/

#include "access.h"
#include "utils.h"

/* ============== Sequential read and write test function ============== */
void seq_read(char * start_addr, long size, long count) {
	asm volatile (
		"mov %[start_addr], %%r9 \n"

"LOOP_SEQLOAD1: \n"
		"xor %%r8, %%r8 \n"
"LOOP_SEQLOAD2: \n"
		"vmovntdqa 0x0(%%r9, %%r8), %%zmm0 \n"
		"add $0x40, %%r8 \n"
		"cmp %[size], %%r8 \n"
		"jl LOOP_SEQLOAD2 \n"

		"add %[size], %%r9 \n"
		"cmp %[end_addr], %%r9 \n"
		"jl LOOP_SEQLOAD1 \n"

		:: [start_addr]"r"(start_addr), [end_addr]"r"(start_addr + count * size), [size]"r"(size)
		:"%r8", "%r9"
	);

}

void seq_write(char * start_addr, long size, long count) {
    asm volatile (
		"mov %[start_addr], %%r9 \n"
		"movq %[start_addr], %%xmm0 \n"

"LOOP_SEQCLWB1: \n"
		"xor %%r8, %%r8 \n"
"LOOP_SEQCLWB2: \n"
		"vmovdqa64  %%zmm0,  0x0(%%r9, %%r8) \n"
		"clwb  (%%r9, %%r8) \n"
		"add $0x40, %%r8 \n"
		"cmp %[size], %%r8 \n"
		"jl LOOP_SEQCLWB2 \n"

		"add %[size], %%r9 \n"
		"cmp %[end_addr], %%r9 \n"
		"jl LOOP_SEQCLWB1 \n"

		:: [start_addr]"r"(start_addr), [end_addr]"r"(start_addr + count * size), [size]"r"(size)
		: "%r8", "%r9"
	);
}

/* ============== random read and write test function ==============*/
void rand_read(char * start_addr, long size, long count) {
	long randnum = getSeed();
	long * rand_seed = &randnum;
	
	static long access_mask = (BUFSIZE - 1) ^ (ALIGN_SIZE- 1);
    
	asm volatile (
		"movabs $0xd800000000000000, %%rcx \n"	/* rcx: bitmask used in LFSR */
		"xor %%r8, %%r8 \n"						/* r8: access offset */
		"xor %%r11, %%r11 \n"					/* r11: access counter */
// 1
"LOOP_READBW_RLOOP: \n"					/* outer (counter) loop */
		RandLFSR64								/* LFSR: uses r9, r12 (reusable), rcx (above), fill r8 */
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"					/* r10: accessed size */
"LOOP_READBW_ONE: \n"					/* inner (access) loop, unroll 8 times */
		LD_AVX512							/* Access: uses r8[rand_base], r10[size_accessed], r9 */
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_READBW_ONE \n"

		"add $1, %%r11 \n"
		"cmp %[count], %%r11\n"
		"jl LOOP_READBW_RLOOP \n"

		: [random]"=r"(rand_seed)
		: [start_addr]"r"(start_addr), [accesssize]"r"(size), [count]"r"(count), "0"(rand_seed), [accessmask]"r"(access_mask)
		: "%rcx", "%r12", "%r11", "%r10", "%r9", "%r8"
	);
}

void rand_write(char * start_addr, long size, long count) {
	long randnum = getSeed();
	long * rand_seed = &randnum;
	static long access_mask = (BUFSIZE - 1) ^ (ALIGN_SIZE- 1);

	asm volatile (
		"movabs $0xd800000000000000, %%rcx \n"
		"xor %%r11, %%r11 \n"
		"movq %[start_addr], %%xmm0 \n"			/* zmm0: read/write register */
// 1
"LOOP_WRITEBW_RLOOP: \n"
		RandLFSR64
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
"LOOP_WRITEBW_ONE: \n"
		WR_AVX512
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_WRITEBW_ONE \n"

		"add $1, %%r11 \n"
		"cmp %[count], %%r11\n"
		"jl LOOP_WRITEBW_RLOOP \n"

		: [random]"=r"(rand_seed)
		: [start_addr]"r"(start_addr), [accesssize]"r"(size), [count]"r"(count), "0"(rand_seed), [accessmask]"r"(access_mask)
		: "%rcx", "%r12", "%r11", "%r10", "%r9", "%r8"
	);
}

void rand_write_lat(char * start_addr, long size, long count) {
	long randnum = getSeed();
	long * rand_seed = &randnum;
	static long access_mask = (BUFSIZE - 1) ^ (ALIGN_SIZE - 1);

	asm volatile (
		"movabs $0xd800000000000000, %%rcx \n"
		"xor %%r11, %%r11 \n"
		"movq %[start_addr], %%xmm0 \n"			/* zmm0: read/write register */
		
"LOOP_FENCED_WRITE: \n"
		RandLFSR64								/* get a random value into r8*/
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
"LOOP_WRITELAT_ONE: \n"
		WR_AVX512
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_WRITELAT_ONE \n"

		LAT_FENCE							/* fence each write operation*/
		"add $1, %%r11 \n"
		"cmp %[count], %%r11\n"
		"jl LOOP_FENCED_WRITE \n"

		: [random]"=r"(rand_seed)
		: [start_addr]"r"(start_addr), [accesssize]"r"(size), [count]"r"(count), "0"(rand_seed), [accessmask]"r"(access_mask)
		: "%rcx", "%r12", "%r11", "%r10", "%r9", "%r8"
	);
}

void rand_read_lat(char * start_addr, long size, long count) {
	long randnum = getSeed();
	long * rand_seed = &randnum;
	static const long access_mask = (BUFSIZE - 1) ^ (ALIGN_SIZE - 1);

	asm volatile (
		"movabs $0xd800000000000000, %%rcx \n"
		"xor %%r11, %%r11 \n"
		"movq %[start_addr], %%xmm0 \n"			/* zmm0: read/write register */
		
"LOOP_FENCED_READ: \n"
		RandLFSR64								/* get a random value into r8*/
		"lea (%[start_addr], %%r8), %%r9 \n"
		"xor %%r10, %%r10 \n"
"LOOP_READLAT_ONE: \n"
		LD_AVX512
		"cmp %[accesssize], %%r10 \n"
		"jl LOOP_READLAT_ONE \n"
		
		LAT_FENCE							/* fence each read operation*/
		"add $1, %%r11 \n"
		"cmp %[count], %%r11\n"
		"jl LOOP_FENCED_READ \n"

		: [random]"=r"(rand_seed)
		: [start_addr]"r"(start_addr), [accesssize]"r"(size), [count]"r"(count), "0"(rand_seed), [accessmask]"r"(access_mask)
		: "%rcx", "%r12", "%r11", "%r10", "%r9", "%r8"
	);
}