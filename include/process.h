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
#include "common.h"

#define PROCESS_NUM 10
#define NOT_EXIST (-999)

int pipe_log_file;
int events_log_file;
timestamp_t my_current_timestamp;

enum {
    SUCCESS = 0,
    EMPTY = 1,
    EMPTY_EOF = 2,
    ERROR = -1
};

enum pipe_log_type {
    CLOSED_READ = 0,
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
timestamp_t calc_timestamp(timestamp_t external_timestamp, timestamp_t internal_counter);
int wait_for_messages_from_everybody(void* void_dest, MessageType supposed_type);
void write_pipe_log_close(int first, int second, int fd, enum pipe_log_type type);
int close_left_pipe_ends(int process_id, int* pipe_write_ends,  int* pipe_read_ends, long processes_num);
void write_pipe_log_open(int first, int second, int fd0, int fd1);
void write_events_log(const char* message, int message_len);

#endif //LAB1_PROCESS_H
