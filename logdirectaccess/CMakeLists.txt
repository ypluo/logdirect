cmake_minimum_required(VERSION 3.11)

project(greenway LANGUAGES CXX C)
include(ExternalProject)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

option(LOGDIRECT_DMABUF "use dmabuf" ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fPIC -pthread -fmax-errors=5 -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

add_link_options(-lpthread)

if(LOGDIRECT_DMABUF)
    message("-- DMABUF: Using CMB-based DMA_BUF")
    add_compile_options(-I/usr/src/linux-headers-5.14.0+/)
    add_compile_definitions(DMABUF)
endif()

ExternalProject_Add(
    YCSB-Gen
    GIT_REPOSITORY "https://gitee.com/ypluo18/YCSB-Gen.git"
    GIT_TAG "master"
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
)

include_directories(include)
include_directories(fontend)
include_directories(database/leveldb/include)
include_directories(database/bitcask/include)

add_subdirectory(database)
add_subdirectory(frontend)

add_executable(server server.cc)
target_link_libraries(server frontend leveldb bitcask)

add_executable(benchmark benchmark.cc)
target_link_libraries(benchmark frontend leveldb bitcask)

add_executable(test test.cc)
target_link_libraries(test frontend leveldb bitcask)