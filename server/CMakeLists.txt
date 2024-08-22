add_executable(server "server_test.cpp")

target_include_directories(server PRIVATE ../include)
target_link_libraries(server Boost::system)