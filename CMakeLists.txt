cmake_minimum_required(VERSION 3.21)
project(lab1 C)

set(CMAKE_C_STANDARD 99)

include_directories(.)

add_compile_options(
        -pedantic
        -Wall)

add_executable(lab1
        pa3/common.h
        pa3/ipc.h
        pa3/pa2345.h
        pa3/process.h
        pa3/ipc.c
        pa3/main.c
        pa3/bank_robbery.c
        pa3/process.c)

add_library(runtime SHARED IMPORTED)

set_target_properties(runtime PROPERTIES IMPORTED_LOCATION ../lib64/libruntime.so)

target_link_libraries(lab1 runtime)