//
// Created by Ana Mun on 17.09.2023.
//

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "ipc.h"
#include "process.h"

local_id last_sender_id;

int send(void *void_source, local_id dst, const Message *msg) {
    struct msg_transfer *source = (struct msg_transfer *) void_source;
    ssize_t written_bytes = write(source->write_ends[dst], msg,
                                  (ssize_t) (sizeof(MessageHeader) + msg->s_header.s_payload_len));
    //printf("try to send from %d to %d via %d result %zd\n", source->id, dst, source->write_ends[dst], written_bytes);
    if (written_bytes < 0) {
        perror("write kek");

        return ERROR;
    } else {
        return SUCCESS;
    }
}

int send_multicast(void *void_source, const Message *msg) {
    struct msg_transfer *source = (struct msg_transfer *) void_source;
    for (int i = 0; i < source->processes_num; i++) {
        if (i == source->id) continue;
        int result = send(void_source, (local_id) i, msg);
        if (result == ERROR) return ERROR;
    }
    return SUCCESS;
}

int receive(void *void_dest, local_id from, Message *msg) {
    struct msg_transfer *dest = (struct msg_transfer *) void_dest;
    long read_result = read(dest->read_ends[from], msg, sizeof(MessageHeader));
    switch (read_result) {
        case -1: // case -1 means pipe is empty and errno = EAGAIN
            if (errno == EAGAIN) {
                return EMPTY;
            } else {
                perror("read");
                return ERROR;
            }
        case 0: // case 0 means all bytes are read and EOF
            return SUCCESS;
        default:
            read_result = read(dest->read_ends[from], ((char*) msg) + sizeof(MessageHeader), msg->s_header.s_payload_len);
            if (read_result < 0) return ERROR;
            return SUCCESS;
    }
}

int receive_any(void *void_dest, Message *msg) {
    struct msg_transfer *dest = (struct msg_transfer *) void_dest;
    while (1) {
        for (int waited_proc_id = 0; waited_proc_id < dest->processes_num; waited_proc_id++) {
            if (waited_proc_id == dest->id) continue;
            int result = receive(dest, (local_id) waited_proc_id, msg);
            switch (result) {
                case SUCCESS:
                    last_sender_id = (local_id) waited_proc_id;
                    return SUCCESS;
                case ERROR:
                    return ERROR;
                default:
                    break;
            }
        }
    }
}

