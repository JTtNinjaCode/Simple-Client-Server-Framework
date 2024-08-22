add_executable(client "client_test.cpp")

target_include_directories(client PRIVATE ../include)
target_link_libraries(server Boost::system)