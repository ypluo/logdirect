// Copyright (c) 2018 The atendb Authors. All rights reserved.

#ifndef ATENDB_FILE_FILE_H_
#define ATENDB_FILE_FILE_H_

#include <string>

namespace atendb {

const int SEGMENT_SIZE = 128 * 1024 * 1024;

class File {
 public:
  File() { }
  virtual ~File() {}

  virtual bool Read(uint32_t offset, uint32_t n, char* buf) = 0;

  virtual bool AppendData(const std::string& key,
			  const std::string& value,
			  uint32_t* file_offset) = 0;

  virtual bool AppendIndex(const std::string& key,
			   uint32_t file_index,
			   uint32_t file_offset,
			   uint32_t key_size_,
			   uint32_t value_size_) = 0;

  virtual bool AppendDataBatch(uint8_t * buf, int length, uint32_t * file_offset) = 0;

  virtual void Sync() = 0;
  
  virtual bool Close() = 0; 

  virtual uint32_t FileOffset() = 0;
};

} // namespace atendb

#endif // ATENDB_FILE_FILE_H_

