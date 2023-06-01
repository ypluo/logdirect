// Copyright (c) 2018 The atendb Authors. All rights reserved.

#include <iostream>
#include <vector>
#include <thread>
#include <dirent.h>
#include <memory>
#include <cstring>

#include "../db/db_impl.h"
#include "../util/coding.h"
#include "db_impl.h"

namespace atendb {

static const Index not_found;

DBImpl::DBImpl(const Options& options) :
    dbname_(options.dbname_),
    directory_(options.dir_),
    file_num_(0),
    max_index_num_(0),
    sync_(options.sync_)
     { 
        global_lock_.Unlock();
     }

DBImpl::~DBImpl() {
    for (auto file : data_files_) {
        delete file;
    }
    for (auto file : index_files_) {
        delete file;
    }
}

bool DBImpl::Put(const std::string& key, const std::string& value) {
    retry_put:
    global_lock_.Lock();
    uint32_t file_index = file_num_ - 1;
    global_lock_.Unlock();
    uint32_t file_offset = 0;
    File* data_file = data_files_[file_index];
    
    auto s = data_file->AppendData(key, value, &file_offset);
    if (likely(s)) {
        if(sync_)
            data_file->Sync(); // sync the write to the disk

        Index idx(file_index, file_offset, key.size(), value.size());
        hash_table_.insert_or_assign(key, idx);
        uint32_t slot = Hash(key.c_str()) % max_index_num_;
        File* index_file = index_files_[slot];
        index_file->AppendIndex(key, idx.fileaddr_.file_index, idx.fileaddr_.file_offset,
                    idx.key_size_, idx.value_size_);
        
        return true;
    } else {
        bool getLock = global_lock_.TryLock();
        if(getLock == true) {
            // create a new data file
            std::string data_fname = directory_ + "/" + dbname_ + "/" + DataFileName + std::to_string(file_num_);
            File* new_data_file;
            Env::NewFile(data_fname, &new_data_file);
            data_files_.push_back(new_data_file);
            
            file_num_ += 1;
            global_lock_.Unlock();
        }

        goto retry_put;
    }

    return false;
}

bool DBImpl::PutBatch(std::vector<std::string> & key_batch, std::vector<Index> & index_batch, 
        uint8_t *buf, int length)
{   
    retry_batch:
    global_lock_.Lock();
    uint32_t file_index = file_num_ - 1;
    global_lock_.Unlock();
    uint32_t file_offset = 0;
    File* data_file = data_files_[file_index];

    auto s = data_file->AppendDataBatch(buf, length, &file_offset);
    if (likely(s)) {
        if(sync_)
            data_file->Sync(); // sync the write to the disk
        
        for(int i = 0; i < index_batch.size(); i++) {
            Index idx = index_batch[i];
            idx.fileaddr_.file_index = file_index;
            idx.fileaddr_.file_offset = file_offset + idx.fileaddr_.file_offset;
            hash_table_.insert_or_assign(key_batch[i], idx);
            uint32_t slot = Hash(key_batch[i].c_str()) % max_index_num_;
            File* index_file = index_files_[slot];
            index_file->AppendIndex(key_batch[i], idx.fileaddr_.file_index, idx.fileaddr_.file_offset,
                    idx.key_size_, idx.value_size_);
        }

        return true;
    } else {
        bool getLock = global_lock_.TryLock();
        if(getLock == true) {
            // create a new data file
            std::string data_fname = directory_ + "/" + dbname_ + "/" + DataFileName + std::to_string(file_num_);
            File* new_data_file;
            Env::NewFile(data_fname, &new_data_file);
            data_files_.push_back(new_data_file);
            
            file_num_ += 1;
            global_lock_.Unlock();
        }
        goto retry_batch;
    }
}

bool DBImpl::Get(const std::string& key, std::string* value) {
    Index idx = hash_table_.find(key);
    if(idx != not_found) {
        if(idx.flag_ == 0) { // in memory
            char buf[idx.DataSize()];
            memcpy(buf, idx.memaddr_, idx.DataSize());
            DecodeData(buf, idx.key_size_, idx.value_size_, value);
            return true;
        } else if(idx.flag_ == 1) { // in file
            assert(idx.fileaddr_.file_index < file_num_);
            File * data_file = data_files_[idx.fileaddr_.file_index];
            char buf[idx.DataSize()];
            auto s = data_file->Read(idx.fileaddr_.file_offset, idx.DataSize(), buf);
            if (likely(s)) {
                DecodeData(buf, idx.key_size_, idx.value_size_, value);
                return true;
            } else {
                return s;
            }
        }
        return false;
    } else { // not found key in the database
        return false;
    }
}

bool DBImpl::Delete(const std::string& key) {
    Index idx = hash_table_.find(key);
    if(idx != not_found) {
        // delete from in-memory index
        hash_table_.erase(key);
        // index_file_->AppendIndex(key, idx.fileaddr_.file_index, idx.fileaddr_.file_offset, idx.key_size_, idx.value_size_);
        return true;
    } else {
        return false;
    }
}

void DBImpl::IndexCallback(File* file) {
    uint32_t file_offset = file->FileOffset();
    if (file_offset != 0) {
        uint32_t pos = 0;
        Index idx;
        std::string key;
        
        while(pos < file_offset) {
            char size_buf[sizeof(uint32_t)];
            auto s = file->Read(pos, sizeof(uint32_t), size_buf);
            if (s) {
                pos += sizeof(uint32_t);
                uint32_t index_size = DecodeFixed32(size_buf);
                char index_buf[index_size];
                auto s = file->Read(pos, index_size, index_buf);
                if (s) {
                    pos += index_size;
                    uint32_t tmp_key_size;
                    DecodeIndex(index_buf, &key, &idx.fileaddr_.file_index, &idx.fileaddr_.file_offset, 
                        &tmp_key_size, &idx.value_size_);
                    idx.key_size_ = tmp_key_size;
                    
                    if(idx.value_size_ > 0) {
                        hash_table_.insert_or_assign(key, idx); 
                    } else {
                        hash_table_.erase(key);
                    }
                }
            }
        }
    }
}

void DBImpl::Recover(const std::string & dirname) {
    file_num_ = 0;

    DIR * dir = opendir(dirname.c_str());
    if (dir == NULL) {
        fprintf(stderr, "opendir error\n");
    }

    std::vector<std::thread> threads;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR) {
            continue;
        } else {
            std::string name = entry->d_name;
            std::string abs_name = dirname + "/" + name;
            if(name == "pmrlog.log") continue;

            uint32_t file_id = stoi(name.substr(name.find_first_of("-") + 1));

            if (name.find(std::string("INDEX")) != std::string::npos) {
                File * index_file;
                Env::NewFile(abs_name, &index_file);
                index_files_[file_id] = index_file;
                
                std::thread th(&DBImpl::IndexCallback, this, index_file);
                threads.push_back(std::move(th));
            } else if (name.find(std::string("DATA")) != std::string::npos) {
                if(file_id + 1 > file_num_) {
                    data_files_.resize(file_id + 1);
                    file_num_ = file_id + 1;
                }

                File * data_file;
                Env::NewFile(abs_name, &data_file);
                data_files_[file_id] = data_file;
            } else {
                fprintf(stderr, "Error filename format\n");
                continue;
            }
        }
    }
    closedir(dir);

    for(auto & th : threads) {
        th.join();
    }

    return;
}


auto DB::Open(const std::string & directory, const std::string & dbname, bool sync) -> std::unique_ptr<DB> {
    const int MAX_INDEX_COUNT = 16;

    // get the database directory
    std::string data_dir = directory + "/" + dbname;
    Options opt(directory, dbname, sync);
    auto impl = std::make_unique<DBImpl>(opt);
    if (!Env::FileExists(data_dir)) {
        // create a new database
        auto s1 = Env::CreateDir(data_dir);
        if (!s1) {
            return nullptr;
        }
        // create first data file
        std::string data_fname = data_dir + DataFileName + std::to_string(0);
        File* first_data;
        auto s2 = Env::NewFile(data_fname, &first_data);
        if (s2) {
            impl->data_files_.push_back(first_data);
        } else {
            return nullptr;
        }
        impl->file_num_ = 1;
        
        impl->max_index_num_ = MAX_INDEX_COUNT;
        impl->index_files_.resize(MAX_INDEX_COUNT);
        for(int i = 0; i < impl->max_index_num_; i++) {
            // create index files
            std::string index_fname = data_dir + IndexFileName + std::to_string(i);
            File* index_file;
            auto s3 = Env::NewFile(index_fname, &index_file);
            if (s3) {
                impl->index_files_[i] = index_file;
            } else {
                return nullptr;
            }
        }
    } else {
        // Recover
        impl->max_index_num_ = MAX_INDEX_COUNT;
        impl->index_files_.resize(MAX_INDEX_COUNT);
        impl->Recover(data_dir);
    }

    return std::move(impl);
}

} // namespace atendb
