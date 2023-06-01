find_library(IBVERB_LIBRARIES NAMES ibverbs)
if(NOT IBVERB_LIBRARIES)
  message(FATAL_ERROR "IBVERB_LIBRARIES library not found!")
endif()

add_library(rdma_util SHARED "rdma_util.cpp")
target_link_libraries(rdma_util ibverbs)

add_executable(rdmatest rdmatest.cc)
target_link_libraries(rdmatest rdma_util sock_util)
