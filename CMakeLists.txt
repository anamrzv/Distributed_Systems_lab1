cmake_minimum_required(VERSION 3.21)
project(lab1 C)

set(CMAKE_C_STANDARD 99)

include_directories(.)

add_compile_options(
        -pedantic
        -Wall)

add_executable(lab1
        include/common.h
        include/ipc.h
        include/pa2345.h
        include/process.h
        src/ipc.c
        src/main.c
        src/bank_robbery.c
        src/process.c
        src/banking.c)

add_library(runtime SHARED IMPORTED)

set_target_properties(runtime PROPERTIES IMPORTED_LOCATION ../lib64/libruntime.so)

target_link_libraries(lab1 runtime)