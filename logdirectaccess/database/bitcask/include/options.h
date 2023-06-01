// Copyright (c) 2018 The atendb Authors. All rights reserved.

#ifndef ATENDB_INCLUDE_OPTIONS_H_
#define ATENDB_INCLUDE_OPTIONS_H_

#include "env.h"

namespace atendb {

class Options {
 public:
  Options() {
    if (!Env::GetCurrentDir(&dir_)) {
      fprintf(stderr, "Error get current directory");
    }
  }

  Options(const std::string & dir, const std::string & dbname, bool sync) { 
    if(dir.size() == 0) {
      if (!Env::GetCurrentDir(&dir_)) {
        fprintf(stderr, "Error get current directory");
      }
    } else {
      dir_ = dir;
    }
    
    dbname_ = dbname;
    sync_ = sync;
  }

  ~Options() {}

  std::string dir_;
  std::string dbname_;
  bool sync_;
};

const Options default_opt;

} // namespace atendb

#endif // ATENDB_INCLUDE_OPTIONS_H_
