cmake_minimum_required(VERSION 2.6)

project(bitcask CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CXX_FLAGS
    -march=native
    -rdynamic
    -std=c++17
    -pthread
)

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS_DEBUG "-std=c++17 -g")

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
message(STATUS "CXX_FLAGS = " ${CMAKE_CXX_FLAGS} " " ${CMAKE_CXX_FLAGS_${BUILD_TYPE}})

# find_package(GTest REQUIRED)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/libcuckoo)

set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin) 

file(GLOB_RECURSE SOURCE 
     ${CMAKE_CURRENT_SOURCE_DIR} 
     ${CMAKE_CURRENT_SOURCE_DIR}/db/*.cc
     ${CMAKE_CURRENT_SOURCE_DIR}/port/*.cc
     ${CMAKE_CURRENT_SOURCE_DIR}/util/*.cc
     ${CMAKE_CURRENT_SOURCE_DIR}/file/*.cc
)

add_library(bitcask ${SOURCE})
target_link_libraries(bitcask pthread rt)
set_target_properties(bitcask PROPERTIES OUTPUT_NAME bitcask)

add_subdirectory(test)
