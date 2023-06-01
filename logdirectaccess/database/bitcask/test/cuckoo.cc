#include <string>
#include <cassert>

#include "../libcuckoo/cuckoohash_map.h"
#include "../include/index.h"

using namespace libcuckoo;

int main(void) {
    const int TEST_SCALE = 1000000;
    cuckoohash_map<std::string, atendb::Index> hash_table;

    std::string prefix = "PREFIX-";
    for(int i = 0; i < TEST_SCALE; i++) {
        atendb::Index idx(i, i, i, i);
        hash_table.insert(prefix + std::to_string(i), idx);
    }

    for(int i = 0; i < TEST_SCALE; i++) {
        atendb::Index res = hash_table.find(prefix + std::to_string(i));
        assert(res.fileaddr_.file_offset == i);
    }

    return 0;
}