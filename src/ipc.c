//
// Created by Ana Mun on 17.09.2023.
//

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "include/ipc.h"
#include "include/process.h"

int send(void* void_source, local_id dst, const Message* msg) {
    struct msg_source* source = (struct msg_source*) void_source;
    ssize_t written_bytes = write(source->write_ends[dst], msg, sizeof(Message));
    if (written_bytes < 0) {
        perror("write");
        return ERROR;
    } else {
        return SUCCESS;
    }
}

int send_multicast(void* void_source, const Message* msg) {
    struct msg_source* source = (struct msg_source*) void_source;
    for (int i = 0; i < source->processes_num; i++) {
        if (i == source->id ) continue;
        ssize_t written_bytes = write(source->write_ends[i], msg, sizeof(Message));
        if (written_bytes < 0) {
            perror("write");
            return ERROR;
        }
    }
    return SUCCESS;
}

int receive(void* void_dest, local_id from, Message* msg) {
    struct msg_destination* dest = (struct msg_destination*) void_dest;
    long read_result = read(dest->read_ends[from], msg, sizeof(Message));
    switch (read_result) {
        case -1: // case -1 means pipe is empty and errno = EAGAIN
            if (errno == EAGAIN) {
                return EMPTY;
            } else {
                perror("read");
                return ERROR;
            }
        case 0: // case 0 means all bytes are read and EOF
            return EMPTY_EOF;
        default:
            return SUCCESS;
    }
}

int receive_any(void* void_dest, Message* msg) {
    struct msg_destination* dest = (struct msg_destination*) void_dest;
    while (1) {
        for (int waited_proc_id = 0; waited_proc_id < dest->processes_num; waited_proc_id++) {
            if (waited_proc_id == dest->id) continue;
            int result = receive(void_dest, (local_id) waited_proc_id, msg);
            switch (result) {
                case SUCCESS:
                    return SUCCESS;
                case ERROR:
                    return ERROR;
                default: //EMPTY or EOF
                    continue;
            }
        }
    }
}

