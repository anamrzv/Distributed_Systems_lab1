//
// Created by Ana Mun on 17.09.2023.
//

#include "include/ipc.h"
#include "include/process.h"


int start_parent(long children_num) {
    long processes_num = children_num + 1;

    int pipe_write_ends[processes_num][processes_num];
    int pipe_read_ends[processes_num][processes_num];
    int fd[2]; // todo: check position

    // open all pipes
    for (size_t from = 0; from < children_num; from++) {
        for (size_t to = 0; to < children_num; to++) {
            if (from != to) {
                if (pipe(fd) < 0) {
                    fprintf(stderr, "Creation of pipe failed.\n"); // todo: close pipes
                    return -1;
                }
                pipe_read_ends[from][to] = fd[0];
                pipe_write_ends[from][to] = fd[1];
            }
        }
    }

    for (int i = 0; i < children_num; i++) {
        pid_t child_pid = fork();
        if (child_pid == -1) {
            printf("fork() failed for process number %d\n", i + 1); // todo: close pipes
            return -1;
        } else if (child_pid > 0) { // we are in parent process
            //leave only its ends
            for (size_t from = 0; from < processes_num; from++) {
                for (size_t to = 0; to < processes_num; to++) {
                    if (from == to) continue;

                    if (from != 0) {
                        close(pipe_write_ends[from][to]);
                    }
                    if (to != 0) {
                        close(pipe_read_ends[from][to]);
                    }
                }
            }
        } else if (child_pid == 0) { // we are in child process
            //leave only its ends
            for (size_t from = 0; from < processes_num; from++) {
                for (size_t to = 0; to < processes_num; to++) {
                    if (from == to) continue;

                    int current_child_num = i + 1;
                    if (from != current_child_num) {
                        close(pipe_write_ends[from][to]);
                    }
                    if (to != current_child_num) {
                        close(pipe_read_ends[from][to]);
                    }
                }
            }

        }
    }


    return 0;
}
