// Copyright (c) 2018 The atendb Authors. All rights reserved.

#ifndef ATENDB_UTIL_LOCK_H_
#define ATENDB_UTIL_LOCK_H_

#include <assert.h>
#include <atomic>
#include <mutex>

namespace atendb {

class SpinLock {
  public:
    void Lock() {
      while(lck.test_and_set(std::memory_order_acquire)){}
    }
    
    void Unlock() {
      lck.clear(std::memory_order_release);
    }
    
    bool TryLock() {
      // if unlocked and try to lock successfully: return true;
      // if already locked and try to lock: return false;
      return lck.test_and_set(std::memory_order_acquire) == false;
    }

  private:
    std::atomic_flag lck = ATOMIC_FLAG_INIT;
};

}  // namespace atendb

#endif // ATENDB_UTIL_LOCK_H_
