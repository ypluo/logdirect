add_executable(db dbimpl_test.cc)
target_link_libraries(db bitcask)

add_executable(cuckoo cuckoo.cc)
target_link_libraries(cuckoo)