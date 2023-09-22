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

    long processes_num = children_num + 1;
    int my_local_id = PARENT_ID;

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
                write_pipe_log(from, to, my_local_id, CLOSED_WRITE);
            }
            if (to != 0) {
                close(pipe_read_ends[from][to]);
                write_pipe_log(from, to, my_local_id, CLOSED_READ);
            }
        }
    }

    for (int i = 1; i < processes_num; i++) {
        pid_t child_pid = fork();
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
                        write_pipe_log(from, to, my_local_id, CLOSED_WRITE);
                    }
                    if (to != current_child_num) {
                        close(pipe_read_ends[from][to]);
                        write_pipe_log(from, to, my_local_id, CLOSED_READ);
                    }
                }
            }
            break;
        }
    }

    return 0;
}

void write_pipe_log(int first, int second, int my_local_id, enum pipe_log_type type) {
    char message[70];
    if (type == OPENED)
        snprintf(message, sizeof(message), "Process %d: Pipe between processes with local ids %d and %d was OPENED\n",
                 my_local_id, first,
                 second);
    else if (type == CLOSED_WRITE)
        snprintf(message, sizeof(message), "Process %d: Pipe's end to WRITE from %d to %d was CLOSED\n", my_local_id,
                 first,
                 second);
    else if (type == CLOSED_READ)
        snprintf(message, sizeof(message), "Process %d: Pipe's end to READ from %d to %d was CLOSED\n", my_local_id,
                 first,
                 second);
    size_t messageLength = strlen(message);
    ssize_t bytesWritten = write(pipe_log_file, message, messageLength);
    if (bytesWritten == -1) {
        printf("Couldn't write log for pipe between processes with local ids %d and %d\n", first, second);
    }
}