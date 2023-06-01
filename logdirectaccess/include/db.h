#pragma once

#include "flags.h"
#include "../database/dbtype.h"

inline DBType * OpenDB(MyOption & opt) {
    if(opt.db_type == "bitcask") {
        return new BitCask(opt.dir, opt.db_type, opt.sync);
    } else if(opt.db_type == "leveldb") {
        return new LevelDB(opt.dir, opt.db_type, opt.sync);
    } else if(opt.db_type == "logdb") {
        return new LogDB(opt.dir, opt.db_type, opt.sync);
    } else if(opt.db_type == "cuckoodb") {
        return new CuckooDB(opt.dir, opt.db_type, opt.sync);
    }else {
        fprintf(stderr, "wrong front type\n");
        return nullptr;
    }
}