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
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include "ipc.h"
#include "pa1.h"

#define PROCESS_NUM 10

int pipe_log_file;
int events_log_file;
timestamp_t my_current_timestamp;

enum pipe_log_type {
    OPENED = 0,
    CLOSED_READ,
    CLOSED_WRITE
};

struct msg_source {
    int id;
    int* write_ends;
    long processes_num;
};

struct msg_destination {
    int id;
    int* read_ends;
    long processes_num;
};

int start_parent(long children_num);
void write_pipe_log(int first, int second, int my_local_id, enum pipe_log_type type);
void write_events_log(const char* message, int message_len);

#endif //LAB1_PROCESS_H
