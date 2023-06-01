// Copyright (c) 2018 The atendb Authors. All rights reserved.

#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <cstring>

#include "posix_file.h"

namespace atendb {

PosixFile::~PosixFile() {
  if (fd_ > 0) {
   Close();
  } 
}

bool PosixFile::Read(uint32_t offset, uint32_t n, char* buf) {
  if (offset > file_offset_) {
    uint32_t pos = offset - file_offset_;
    if (pos > map_size_) {
      return false;
    } else {
      memcpy(buf, base_buf_+pos, n);
    }
  } else {
    size_t read = pread(fd_, buf, n, offset);
    if (read < 0) {
      return false;
    }
  }
  return true;
}

bool PosixFile::AppendData(const std::string& key,
			   const std::string& value,
			   uint32_t* file_offset) {
  uint32_t data_size = key.size() + value.size();
  char data_buf[data_size];
  EncodeData(data_buf, key, value);

  uint32_t old_offset = file_offset_.load(std::memory_order_relaxed);
  if(old_offset >= SEGMENT_SIZE) {
    return false;
  }
  while (file_offset_.compare_exchange_weak(old_offset, old_offset + data_size,
          std::memory_order_release, std::memory_order_relaxed) == false) {
    if(old_offset >= SEGMENT_SIZE) {
      return false;
    }
  }
  *file_offset = old_offset;

  int64_t count = pwrite(fd_, data_buf, data_size, *file_offset);

  return count == data_size;
}

bool PosixFile::AppendDataBatch(uint8_t * buf, int length, uint32_t * file_offset) {
  uint32_t old_offset = file_offset_.load(std::memory_order_relaxed);
  if(old_offset >= SEGMENT_SIZE) {
    return false;
  }
  while (file_offset_.compare_exchange_weak(old_offset, old_offset + length,
          std::memory_order_release, std::memory_order_relaxed) == false) {
    if(old_offset >= SEGMENT_SIZE) {
      return false;
    }
  }
  *file_offset = old_offset;

  int64_t count = pwrite(fd_, buf, length, *file_offset);

  if(count == length) {
    return true;
  } else {
    fprintf(stderr, "Write to File Error\n");
    exit(-1);
  }
}

bool PosixFile::AppendIndex(const std::string& key,
			    uint32_t file_index,
			    uint32_t file_offset,
			    uint32_t key_size,
			    uint32_t value_size) {
  uint32_t index_size = key.size() + sizeof(uint32_t) * 5;
  char index_buf[index_size];
  EncodeIndex(index_buf, key, file_index,
	      file_offset, key_size, value_size);

  uint32_t offset = file_offset_.fetch_add(index_size);
  ssize_t n = pwrite(fd_, index_buf, index_size, offset);
  return n == index_size;
}

void PosixFile::Sync() {
  fsync(fd_);
}

bool PosixFile::Close() {
  ::close(fd_);
  fd_ = -1;
  return true;
}

} // namespace atendb
