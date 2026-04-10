cmake_minimum_required(VERSION 3.10)

project(${CMAKE_SOURCE_DIR} C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_COMPILER gcc)

add_executable(test src/main.c
        include/bitframe_context.h
        src/bitframe_context.c)

target_include_directories(test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test PRIVATE m)

target_compile_options(test PRIVATE -O3 -Ofast)
