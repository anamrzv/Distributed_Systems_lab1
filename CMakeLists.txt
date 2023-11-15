cmake_minimum_required(VERSION 3.21)
project(lab1 C)

set(CMAKE_C_STANDARD 99)

include_directories(.)

add_compile_options(
        -pedantic
        -Wall)

add_executable(lab1
        pa2/common.h
        pa2/ipc.h
        pa2/pa2345.h
        pa2/process.h
        pa2/ipc.c
        pa2/main.c
        pa2/bank_robbery.c
        pa2/process.c
       )

add_library(runtime SHARED IMPORTED)

set_target_properties(runtime PROPERTIES IMPORTED_LOCATION ../lib64/libruntime.so)

target_link_libraries(lab1 runtime)