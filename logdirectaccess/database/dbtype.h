#ifndef __GREENWAY_DBTYPE__
#define __GREENWAY_DBTYPE__

#include "bitcask/include/db.h"

#include "leveldb/db.h"
#include "leveldb/cache.h"
#include "leveldb/comparator.h"
#include "leveldb/filter_policy.h"
#include "leveldb/env.h"
#include "leveldb/write_batch.h"
#include "logdb/logfile.h"
#include "cuckoodb/libcuckoo/cuckoohash_map.h"

using DBIndex = atendb::Index; // index for requests in the pmr-based buffer

class DBType {
public:
    virtual bool Put(std::string & key, std::string & val) = 0;

    virtual bool Get(std::string & key, std::string * val) = 0;

    virtual bool Update(std::string & key, std::string & val) = 0;

    virtual bool Delete(std::string & key) = 0;

    virtual bool PutBatch(std::vector<std::string> & key_batch, std::vector<DBIndex> & index_batch, 
                    uint8_t *buf, int length) = 0;
    
    virtual void SetSync() {}
};

class BitCask : public DBType {
private:
    std::unique_ptr<atendb::DB> db_;

public:
    BitCask(const std::string & directory, const std::string & dbname, bool sync) {
        db_ = atendb::DB::Open(directory, dbname, sync);
    }

    bool Put(std::string & key, std::string & val) {
        return db_->Put(key, val);
    }

    bool Get(std::string & key, std::string * val) {
        return db_->Get(key, val);
    }

    bool Update(std::string & key, std::string & val) {
        return db_->Put(key, val);
    }

    bool Delete(std::string & key) {
        return db_->Delete(key);
    }

    bool PutBatch(std::vector<std::string> & key_batch, std::vector<DBIndex> & index_batch, 
                    uint8_t *buf, int length) {
        db_->PutBatch(key_batch, index_batch, buf, length);
        return true;
    }
};

class LevelDB : public DBType {
private:
    static const bool FLAGS_use_existing_db = false;
    static const bool FLAGS_reuse_logs = true;
    static const size_t FLAGS_cache_size = (size_t)256 * 1024 * 1024; // 256 MiB data cache
    static const size_t FLAGS_bloom_bits = 10;

    leveldb::Cache* cache_;
    const leveldb::FilterPolicy* filter_policy_;
    leveldb::DB* db_;
    leveldb::Env* env_;
    leveldb::WriteOptions write_options_;

public:
    LevelDB (const std::string & directory, const std::string & dbname, bool sync) :
            cache_(FLAGS_cache_size >= 0 ? leveldb::NewLRUCache(FLAGS_cache_size) : nullptr),
            filter_policy_(FLAGS_bloom_bits >= 0 ? leveldb::NewBloomFilterPolicy(FLAGS_bloom_bits) : nullptr) {
        leveldb::Options options;
        env_ = leveldb::Env::Default();
        write_options_ = leveldb::WriteOptions();
        write_options_.sync = sync;

        options.env = env_;
        options.create_if_missing = !FLAGS_use_existing_db;
        options.block_cache = cache_;
        options.write_buffer_size = 64 * 1024 * 1024;
        options.max_file_size = 64 * 1024 * 1024;
        options.block_size = leveldb::Options().block_size;
        options.max_open_files = leveldb::Options().max_open_files;
        options.filter_policy = filter_policy_;
        options.reuse_logs = FLAGS_reuse_logs;
        options.compression = leveldb::kNoCompression;
        
        leveldb::Status s = leveldb::DB::Open(options, directory + "/" + dbname, &db_);
        if (!s.ok()) {
            std::fprintf(stderr, "open error: %s\n", s.ToString().c_str());
            std::exit(1);
        }
    }

    bool Put(std::string & key, std::string & val) {
        leveldb::WriteBatch batch;
        batch.Put(leveldb::Slice(key), leveldb::Slice(val));
        leveldb::Status s = db_->Write(write_options_, &batch);
        return s.ok();
    }

    bool Get(std::string & key, std::string * val) {
        leveldb::ReadOptions options;
        leveldb::Status s = db_->Get(options, leveldb::Slice(key), val);
        return s.ok();
    }

    bool Update(std::string & key, std::string & val) {
        leveldb::WriteBatch batch;
        batch.Put(leveldb::Slice(key), leveldb::Slice(val));
        leveldb::Status s = db_->Write(write_options_, &batch);
        return s.ok();
    }

    bool Delete(std::string & key) {
        leveldb::WriteBatch batch;
        batch.Delete(leveldb::Slice(key));
        leveldb::Status s = db_->Write(write_options_, &batch);
        return s.ok();
    }

    bool PutBatch(std::vector<std::string> & key_batch, std::vector<DBIndex> & index_batch, 
                    uint8_t *buf, int length) {
        leveldb::WriteBatch batch;
        for(int i = 0; i < key_batch.size(); i++) {
            int val_size = index_batch[i].value_size_;
            char * val = (char *)buf + index_batch[i].fileaddr_.file_offset + index_batch[i].key_size_;
            batch.Put(leveldb::Slice(key_batch[i]), leveldb::Slice(val, val_size));
        }
        leveldb::Status s = db_->Write(write_options_, &batch);

        return true;
    }
};

class LogDB : public DBType {
private:
    logfiledb::LogFileDB * db_;

public:
    LogDB(const std::string & directory, const std::string & dbname, bool sync) {
        db_ = new logfiledb::LogFileDB(directory, dbname, sync);
    }

    ~LogDB() {
        delete db_;
    }

    bool Put(std::string & key, std::string & val) {
        db_->Put(key, val);
        return true;
    }

    bool Get(std::string & key, std::string * val) {
        return true;
    }

    bool Update(std::string & key, std::string & val) {
        db_->Put(key, val);
        return true;
    }

    bool Delete(std::string & key) {
        return true;
    }

    bool PutBatch(std::vector<std::string> & key_batch, std::vector<DBIndex> & index_batch, 
                    uint8_t *buf, int length) {
        db_->PutBatch((char *)buf, length);
        return true;
    }
};

class CuckooDB : public DBType {
private:
    typedef libcuckoo::cuckoohash_map<std::string, std::string> HashTable;
    HashTable * db_;
    logfiledb::LogFileDB * log_;

public:
    
    CuckooDB(const std::string & directory, const std::string & dbname, bool sync) {
        db_ = new HashTable();
        log_ = new logfiledb::LogFileDB(directory, dbname, sync);
    }

    ~CuckooDB() {
        delete db_;
    }

    bool Put(std::string & key, std::string & val) {
        db_->insert_or_assign(key, val);
        log_->Put(key, val);
        return true;
    }

    bool Get(std::string & key, std::string * val) {
        *val = db_->find(key);
        return true;
    }

    bool Update(std::string & key, std::string & val) {
        db_->insert_or_assign(key, val);
        log_->Put(key, val);
        return true;
    }

    bool Delete(std::string & key) {
        db_->erase(key);
        return true;
    }

    bool PutBatch(std::vector<std::string> & key_batch, std::vector<DBIndex> & index_batch, 
                    uint8_t *buf, int length) {
        
        for(int i = 0; i < key_batch.size(); i++) {
            std::string val((char *)buf + index_batch[i].fileaddr_.file_offset + index_batch[i].key_size_, index_batch[i].value_size_);
            db_->insert_or_assign(key_batch[i], val);
        }
        
        log_->PutBatch((char *)buf, length);
        return true;
    }

    void SetSync() {
        log_->SetSync(true);
        fprintf(stderr, "Set Sync to true\n");
    }
};

#endif // DBTYPE