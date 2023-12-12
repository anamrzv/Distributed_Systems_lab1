cmake_minimum_required(VERSION 3.21)
project(Distributed_Systems_lab1 C)

set(CMAKE_C_STANDARD 99)

include_directories(.)

add_compile_options(
        -pedantic
        -Wall)

add_executable(Distributed_Systems_lab1
        pa5/common.h
        pa5/ipc.h
        pa5/pa2345.h
        pa5/process.h
        pa5/ipc.c
        pa5/main.c
        pa5/process.c)

add_library(runtime SHARED IMPORTED)

set_target_properties(runtime PROPERTIES IMPORTED_LOCATION ../lib64/libruntime.so)

target_link_libraries(Distributed_Systems_lab1 runtime)