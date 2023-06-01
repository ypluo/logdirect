add_library(sock_util SHARED "socketutil.cc")

add_executable(socktest socktest.cc)
target_link_libraries(socktest sock_util)