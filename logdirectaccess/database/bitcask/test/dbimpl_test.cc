// Copyright (c) 2018 The atendb Authors. All rights reserved.

#include <iostream>
#include <string>
#include <random>
#include <unistd.h>
#include <algorithm>
#include <thread>
#include <map>

#include "../include/options.h"
#include "../include/db.h"

using namespace atendb;

std::string random_string( size_t length ) {
  auto randchar = []() -> char {
    const char charset[] = "0123456789"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[ rand() % max_index ];
  };
  std::string str(length,0);
  std::generate_n( str.begin(), length, randchar );
  return str;
}

int main() {
  Options opts("~/logdirect/build", "testdb", true);
  
  const int test_scale = 64 * 1024;
  auto db = DB::Open(opts.dir_, opts.dbname_);
  std::map<std::string, std::string> kvs;
  for (int32_t i = 0; i < test_scale; i++) {
    std::string key = random_string(8);
    std::string value = random_string(4088); 
    kvs[key] = value;
    auto s = db->Put(key, value);
    if (!s) {
      std::cout << "Write failed." << key << std::endl;
    }

    std::string read_value;
    s = db->Get(key, &read_value); 
    if (!s || value != read_value) {
      std::cout << "Read value failed: " << key << std::endl;
    }
  }
  db.reset(nullptr);

  auto reopen_db = DB::Open(opts.dir_, opts.dbname_);
  for (auto it = kvs.begin(); it != kvs.end(); ++it) {
    std::string read_value;
    auto s = reopen_db->Get(it->first, &read_value); 
    if (!s || it->second != read_value) {
      std::cout << "Reread value failed: " << it->first << std::endl;
    }
  }

  // Delete
  for (auto it = kvs.begin(); it != kvs.end(); ++it) {
    auto s = reopen_db->Delete(it->first);
    if (!s) {
      std::cout << "Delete value failed: " << it->first << std::endl;
    }
  }

  for (auto it = kvs.begin(); it != kvs.end(); ++it) {
    std::string read_value;
    auto s = reopen_db->Get(it->first, &read_value);
    if (s || read_value != "") {
      std::cout << "Delete value failed: " << it->first << std::endl;
    }
  }
  reopen_db.reset(nullptr);;

  return 0;
}
