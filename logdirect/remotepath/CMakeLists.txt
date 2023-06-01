cmake_minimum_required(VERSION 3.13)

project(remotepath)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread -mclflushopt")

option(REMOTEPATH_DMABUF "use dmabuf" ON)
if(REMOTEPATH_DMABUF)
    message("-- DMABUF: Using CMB-based DMA_BUF")
    add_compile_options(-I/usr/src/linux-headers-5.14.0+/)
    add_compile_definitions(DMABUF)
endif()

find_library(PMEMLIB NAMES pmem)

add_link_options(-libverbs -lpthread)

include_directories(include)
include_directories(rdmautil)
include_directories(socketutil) 

add_subdirectory(socketutil)
add_subdirectory(rdmautil)

add_library(path_util SHARED "path.cc")
target_link_libraries(path_util rdma_util sock_util pmem)

add_executable(latency latency.cc)
target_link_libraries(latency path_util)

add_executable(throughput throughput.cc)
target_link_libraries(throughput path_util)