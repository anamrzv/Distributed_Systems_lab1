//
// Created by Ana Mun on 17.09.2023.
//

#ifndef LAB1_PROCESS_H
#define LAB1_PROCESS_H

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

struct child_process {
    uint8_t local_id;
    pid_t pid;
};

struct parent_process {
    uint8_t local_id;
    pid_t pid;
    struct child_process* children;  //указатель на начало массива
};

int get_process_id();
int start_parent(long children_num);

#endif //LAB1_PROCESS_H
