add_subdirectory(rdmautil)

set(FRONT_SRC sync/sync.cc hybrid/hybrid.cc group/group.cc pmem/pmem.cc)
add_library(frontend ${FRONT_SRC})
target_link_libraries(frontend rdma)