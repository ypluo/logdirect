#ifndef __GLOBAL_UTIL__
#define __GLOBAL_UTIL__
#include <stdlib.h>
#include <sys/time.h>
#include <cstring>

const int CACHE_LINE_SIZE = 64;
const long BUFSIZE = (4 * 1024 * 1024); // 4MiB
const int ALIGN_SIZE = 64;

inline double seconds()
{
  timeval now;
  gettimeofday(&now, NULL);
  return now.tv_sec + now.tv_usec/1000000.0;
}

static inline int getSeed() {
	timeval now;
	gettimeofday(&now, NULL);
	return now.tv_usec;
}

static inline void drop_cache() {
    char * tmp_buf = (char *)malloc(1 << 27); // 128 MB
    memset(tmp_buf, 0x5a, (1 << 27));
    free(tmp_buf);
    asm volatile("sfence" ::: "memory");
}

#endif // __GLOBAL_UTIL__