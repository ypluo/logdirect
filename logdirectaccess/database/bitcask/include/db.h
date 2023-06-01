// Copyright (c) 2018 The atendb Authors. All rights reserved.

#ifndef ATENDB_INCLUDE_DB_H_
#define ATENDB_INCLUDE_DB_H_

#include "options.h"
#include "env.h"
#include "index.h"

namespace atendb {

class DB {
 public:
  DB() { }
  virtual ~DB() {}

  static auto Open(const std::string & directory, const std::string & dbname, bool sync = false) -> std::unique_ptr<DB>;

  virtual bool Put(const std::string& key, const std::string& value) = 0;

  virtual bool PutBatch(std::vector<std::string> & key_batch, std::vector<Index> & index_batch, 
                  uint8_t * buf, int length) = 0;
                
  virtual void PutIndex(const std::string& key, Index & idx) = 0;

  virtual bool Get(const std::string& key, std::string* value) = 0;
  
  virtual bool Delete(const std::string& key) = 0;

 private:
  // No copying allowed
  void operator=(const DB&) = delete;
  DB(const DB&);
};

} // namespace atendb

#endif // ATENDB_INCLUDE_DB_H_
