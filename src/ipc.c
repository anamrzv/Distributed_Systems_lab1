//
// Created by Ana Mun on 17.09.2023.
//


#include "include/ipc.h"
#include "include/process.h"
#include "include/common.h"


int start_parent(long children_num) {
    pipe_log_file = open(pipes_log, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (pipe_log_file == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    events_log_file = open(events_log, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (events_log_file == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    long processes_num = children_num + 1;
    int my_local_id = PARENT_ID;
    my_current_timestamp = 0;

    int pipe_write_ends[processes_num][processes_num];
    int pipe_read_ends[processes_num][processes_num];
    int fd[2]; // todo: check position

    // open all pipes
    for (int from = 0; from < processes_num; from++) {
        for (int to = 0; to < processes_num; to++) {
            if (from != to) {
                if (pipe(fd) < 0) {
                    fprintf(stderr, "Creation of pipe failed.\n"); // todo: close pipes
                    return -1;
                }
                my_current_timestamp++;
                write_pipe_log(from, to, PARENT_ID, OPENED);

                pipe_read_ends[from][to] = fd[0];
                pipe_write_ends[from][to] = fd[1];
            }
        }
    }

    for (int from = 0; from < processes_num; from++) {
        for (int to = 0; to < processes_num; to++) {
            if (from == to) continue;

            if (from != 0) {
                close(pipe_write_ends[from][to]);
                my_current_timestamp++;
                write_pipe_log(from, to, my_local_id, CLOSED_WRITE);
            }
            if (to != 0) {
                close(pipe_read_ends[from][to]);
                my_current_timestamp++;
                write_pipe_log(from, to, my_local_id, CLOSED_READ);
            }
        }
    }

    for (int i = 1; i < processes_num; i++) {
        pid_t child_pid = fork();
        my_current_timestamp++;
        printf("%d | Child process %d is forked", my_current_timestamp, i);
        if (child_pid == -1) {
            printf("fork() failed for process number %d\n", i + 1); // todo: close pipes
            return -1;
        } else if (child_pid == 0) { // we are in child process
            //leave only its ends
            int current_child_num = i;
            my_local_id = current_child_num;
            for (int from = 0; from < processes_num; from++) {
                for (int to = 0; to < processes_num; to++) {
                    if (from == to) continue;

                    if (from != current_child_num) {
                        close(pipe_write_ends[from][to]);
                        my_current_timestamp++;
                        write_pipe_log(from, to, my_local_id, CLOSED_WRITE);
                    }
                    if (to != current_child_num) {
                        close(pipe_read_ends[from][to]);
                        my_current_timestamp++;
                        write_pipe_log(from, to, my_local_id, CLOSED_READ);
                    }
                }
            }

            //todo: send start
            my_current_timestamp++;
            Message* start_msg = malloc(sizeof (Message));
            start_msg->s_header.s_magic = MESSAGE_MAGIC;
            start_msg->s_header.s_type = STARTED;
            start_msg->s_header.s_local_time = my_current_timestamp;
            int length = snprintf(start_msg->s_payload, MAX_PAYLOAD_LEN, log_started_fmt,
                     my_local_id, getpid(), getppid());
            start_msg->s_header.s_payload_len = length;

            struct msg_source src = {my_local_id, pipe_write_ends[my_local_id], processes_num };
            send_multicast(&src, start_msg);

            //todo: wait for all

            //todo: send end
            break;
        }
    }


    return 0;
}

int send_multicast(void* void_source, const Message* msg) {
    struct msg_source* source = (struct msg_source*) void_source;
    for (int i = 0; i < source->processes_num; i++) {
        write_events_log(msg->s_payload, msg->s_header.s_payload_len);
        write(source->write_ends[i], msg, sizeof(Message));
    }
    return 0;
}

int send(void* self, local_id dst, const Message* msg) {

    return 0;
}

int calc_timestamp(int external_timestamp, int internal_counter) {
    if (external_timestamp > internal_counter) return external_timestamp + 1;
    else return internal_counter + 1;
}

void write_events_log(const char* message, int message_len) {
    ssize_t bytes_written = write(events_log_file, message, message_len);
    if (bytes_written == -1) {
        printf("Couldn't write event log");
    }
}

void write_pipe_log(int first, int second, int my_local_id, enum pipe_log_type type) {
    char message[70];
    if (type == OPENED)
        snprintf(message, sizeof(message), "%d | Process %d: Pipe between processes with local ids %d and %d was OPENED\n",
                 my_current_timestamp,
                 my_local_id, first,
                 second);
    else if (type == CLOSED_WRITE)
        snprintf(message, sizeof(message), "%d | Process %d: Pipe's end to WRITE from %d to %d was CLOSED\n",
                 my_current_timestamp, my_local_id,
                 first,
                 second);
    else if (type == CLOSED_READ)
        snprintf(message, sizeof(message), "%d | Process %d: Pipe's end to READ from %d to %d was CLOSED\n",
                 my_current_timestamp, my_local_id,
                 first,
                 second);
    size_t message_length = strlen(message);
    ssize_t bytes_written = write(pipe_log_file, message, message_length);
    if (bytes_written == -1) {
        printf("Couldn't write log for pipe between processes with local ids %d and %d\n", first, second);
    }
}
