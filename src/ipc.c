//
// Created by Ana Mun on 17.09.2023.
//

#include "include/ipc.h"
#include "include/process.h"

int start_parent(long children_num) {
    struct parent_process* parent_process = malloc(sizeof(struct parent_process));
    parent_process->pid = getpid();
    parent_process->local_id = PARENT_ID;

    struct child_process* children;
    children = (struct child_process*) malloc(children_num * sizeof(struct child_process));
    if (children == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return -1;
    }
    parent_process->children = children;

    for (size_t i = 0; i < children_num; i++) {
        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork() failed");
            return -1;
        } else {
//            struct child_process* new_process = malloc(sizeof(struct child_process));
//            new_process->pid = child_pid;
//            new_process->local_id = get_process_id();
            parent_process->children[i].pid = child_pid;
            parent_process->children[i].local_id = get_process_id();
        }
    }
    return 0;
}
