cmake_minimum_required(VERSION 3.5)
project(dealloc_bench C)

set(CMAKE_C_STANDARD 11)

add_executable(dealloc_bench src/main.c)
target_link_libraries(${PROJECT_NAME} rt pthread "/usr/local/lib/libhdr_histogram.so")