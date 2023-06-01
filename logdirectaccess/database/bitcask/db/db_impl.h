// Copyright (c) 2018 The atendb Authors. All rights reserved.

#ifndef ATENDB_DB_DB_IMPL_H_
#define ATENDB_DB_DB_IMPL_H_

#include <atomic>
#include <algorithm>
#include <vector>

#include "../include/index.h"
#include "../include/db.h"
#include "../include/options.h"
#include "../file/file.h"
#include "../util/lock.h"
#include "../util/hash.h"
#include "../libcuckoo/cuckoohash_map.h"

namespace atendb {

const std::string IndexFileName = "/INDEX-";
const std::string DataFileName  = "/DATA-";

using Position = std::pair<uint32_t, uint32_t>;
using HashType = libcuckoo::cuckoohash_map<std::string, Index>;

class DBImpl : public DB {
 public:
  DBImpl(const Options& options);

  ~DBImpl();

  bool Put(const std::string& key, const std::string& value) override final;

  bool PutBatch(std::vector<std::string> & key_batch, std::vector<Index> & index_batch, uint8_t * buf, int length) override final;

  bool Get(const std::string& key, std::string* value) override final;

  bool Delete(const std::string& key) override final;

  void IndexCallback(File* file);

  void Recover(const std::string & dirname);

  void PutIndex(const std::string& key, Index & idx) {
    hash_table_.insert_or_assign(key, idx);
  }

 private:
  friend DB;

  std::string dbname_;
  HashType hash_table_;
  SpinLock global_lock_;
  std::string directory_;
  bool sync_;

  std::vector<File* > data_files_;
  std::vector<File* > index_files_;
  int max_index_num_;
  uint32_t file_num_;

  // No copying allowed
  void operator=(const DBImpl&) = delete;
  DBImpl(const DBImpl&) = delete;
};

} // namespace atendb

#endif
