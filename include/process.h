//
// Created by Ana Mun on 17.09.2023.
//

#ifndef LAB1_PROCESS_H
#define LAB1_PROCESS_H

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int pipe_log_file;

enum pipe_log_type {
    OPENED = 0,
    CLOSED_READ,
    CLOSED_WRITE
};

int start_parent(long children_num);
void write_pipe_log(int first, int second, int my_local_id, enum pipe_log_type type);

#endif //LAB1_PROCESS_H
