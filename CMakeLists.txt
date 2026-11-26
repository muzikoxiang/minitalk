cmake_minimum_required(VERSION 3.16)

project(minitalk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(minitalk_server src/server.cpp)
add_executable(minitalk_client src/client.cpp)

target_compile_options(minitalk_server PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(minitalk_client PRIVATE -Wall -Wextra -Wpedantic)

add_executable(
    minitalk_epoll_server
    src/server_epoll.cpp
    src/database.cpp
    src/database_worker.cpp
)

target_compile_options(
    minitalk_epoll_server
    PRIVATE -Wall -Wextra -Wpedantic
)

find_path(MYSQLCPPCONN_INCLUDE_DIR mysql_driver.h REQUIRED)
find_library(MYSQLCPPCONN_LIBRARY mysqlcppconn REQUIRED)

add_executable(
    minitalk_db_demo
    src/db_demo.cpp
    src/database.cpp
    src/database_worker.cpp
)

target_include_directories(
    minitalk_db_demo
    PRIVATE ${MYSQLCPPCONN_INCLUDE_DIR}
)

target_link_libraries(
    minitalk_db_demo
    PRIVATE ${MYSQLCPPCONN_LIBRARY}
)

target_compile_options(
    minitalk_db_demo
    PRIVATE -Wall -Wextra -Wpedantic
)
target_include_directories(
    minitalk_epoll_server
    PRIVATE ${MYSQLCPPCONN_INCLUDE_DIR}
)

target_link_libraries(
    minitalk_epoll_server
    PRIVATE ${MYSQLCPPCONN_LIBRARY}
)