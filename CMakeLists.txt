cmake_minimum_required(VERSION 3.21)
project(Distributed_Systems_lab1 C)

set(CMAKE_C_STANDARD 99)

include_directories(.)

add_compile_options(
        -pedantic
        -Wall)

add_executable(Distributed_Systems_lab1
        pa4/common.h
        pa4/ipc.h
        pa4/pa2345.h
        pa4/process.h
        pa4/ipc.c
        pa4/main.c
        pa4/process.c
        pa4/priority_queue.c
        pa4/priority_queue.h)

add_library(runtime SHARED IMPORTED)

set_target_properties(runtime PROPERTIES IMPORTED_LOCATION ../lib64/libruntime.so)

#target_link_libraries(Distributed_Systems_lab1 runtime)