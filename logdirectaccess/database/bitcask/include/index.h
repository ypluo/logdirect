// Copyright (c) 2018 The atendb Authors. All rights reserved.

#ifndef ATENDB_INCLUDE_INDEX_H_
#define ATENDB_INCLUDE_INDEX_H_

#include <string>

namespace atendb {

struct Index {
    struct FileAddress {
        uint32_t file_offset = 0;
        uint32_t file_index = 0;
    };

    union {
        FileAddress fileaddr_;
        void * memaddr_;
    };
    uint32_t flag_     :  1;
    uint32_t key_size_ : 31;
    uint32_t value_size_;

    Index() {
        memaddr_ = 0;
        flag_ = 0;
        key_size_ = 0;
        value_size_ = 0;
    }

    Index(uint32_t file_index, uint32_t file_offset, uint32_t key_size, uint32_t value_size) {
        flag_ = 1;
        fileaddr_.file_index = file_index;
        fileaddr_.file_offset = file_offset;
        key_size_ = key_size;
        value_size_ = value_size;
    }

    Index(void * mem, uint32_t key_size, uint32_t value_size) {
        flag_ = 0;
        memaddr_ = mem;
        key_size_ = key_size;
        value_size_ = value_size;
    }

    bool operator!=(const Index& index) {
        return this->memaddr_ != index.memaddr_ || this->flag_ != index.flag_;
        this->key_size_ != index.key_size_ || this->value_size_ != index.value_size_;
    }

    uint32_t DataSize() const { return key_size_ + value_size_; }

    void SetDeleted() {
        key_size_ = 0;
        value_size_ = 0;
    }
};

// class Index {
//  public:
//     Index() :
//         file_index_(0),
//         file_offset_(0),
//         key_size_(0),
//         value_size_(0) {}

//     Index(uint32_t file_index, uint32_t file_offset, uint32_t key_size, uint32_t value_size) :
//         file_index_(file_index),
//         file_offset_(file_offset),
//         key_size_(key_size),
//         value_size_(value_size) {}

//     ~Index() {} 

//     uint32_t FileIndex() const { return file_index_; }

//     uint32_t FileOffset() const { return file_offset_; }

//     uint32_t KeySize() const { return key_size_; }

//     uint32_t ValueSize() const { return value_size_; }

//     uint32_t DataSize() const { return key_size_ + value_size_; }

//     void SetDeleted() {
//         this->key_size_ = 0;
//         this->value_size_ = 0;
//     }

//     void operator=(const Index& index) {
//      this->file_index_ = index.FileIndex();
//      this->file_offset_ = index.FileOffset();
//      this->key_size_ = index.KeySize();
//      this->value_size_ = index.ValueSize();
//     }

//     bool operator!=(const Index& index) {
//      return this->file_index_ != index.FileIndex() || this->file_offset_ != index.FileOffset() ||
//      this->key_size_ != index.KeySize() || this->value_size_ != index.ValueSize();
//     }

//  private:
//     friend class DB;
//     friend class DBImpl;
    
//     uint32_t file_index_;
//     uint32_t file_offset_;
//     uint32_t key_size_;
//     uint32_t value_size_;
// };

} // namespace atendb

#endif // ATENDB_INCLUDE_INDEX_H_