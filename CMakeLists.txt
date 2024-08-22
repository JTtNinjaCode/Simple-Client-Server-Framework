cmake_minimum_required(VERSION 3.12)
project(Simple-Client-Server-Framework)

cmake_policy(SET CMP0079 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Boost 1.85 REQUIRED COMPONENTS system)

if(Boost_FOUND)
  message("find out Boost.")
  include_directories(${Boost_INCLUDE_DIRS})
endif()

add_subdirectory(server)
add_subdirectory(client)