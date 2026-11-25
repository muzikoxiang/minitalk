cmake_minimum_required(VERSION 3.16)

project(minitalk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(minitalk_server src/server.cpp)
add_executable(minitalk_client src/client.cpp)

target_compile_options(minitalk_server PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(minitalk_client PRIVATE -Wall -Wextra -Wpedantic)