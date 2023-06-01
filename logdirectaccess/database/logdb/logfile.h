#pragma once

#include <string>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>

namespace logfiledb {

class LogFileDB {
private:
    int fd_;
    bool sync_;

public:
    LogFileDB(const std::string & directory, const std::string & dbname, bool sync) {
        std::string dir = directory + "/" + dbname;
        if (access(dir.c_str(), F_OK) != 0) {
            mkdir(dir.c_str(), 0755);
        }
        std::string filename = dir + "/" + "log.db";
        fd_ = open(filename.c_str(), O_RDWR | O_CREAT, 0645);
        int ret = ftruncate(fd_, 8UL * 1024 * 1024 * 1024);
        assert(ret == 0);
        lseek(fd_ , 0 , SEEK_SET);
        sync_ = sync;
    }

    ~LogFileDB() {
        close(fd_);
    }

    void Put(std::string & key, std::string & val) {
        char buffer[8 * 1024];
        *((int *)(buffer)) = key.size();
        *((int *)(buffer + 4)) = val.size();
        memcpy(buffer + 8, key.c_str(), key.size());
        memcpy(buffer + 8 + key.size(), val.c_str(), val.size());

        auto ret = write(fd_, buffer, 8 + key.size() + val.size());
        if(sync_) fdatasync(fd_);
        assert(ret == 8 + key.size() + val.size());
    }

    void PutBatch(char * buffer, int length) {
        auto ret = write(fd_, buffer, length);
        if(sync_) fdatasync(fd_);
        assert(ret == length);
    }

    void SetSync(bool sync_state) {
        sync_ = sync_state;
        fdatasync(fd_);
    }
};

} // namespace logfile