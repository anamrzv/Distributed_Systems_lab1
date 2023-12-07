//
// Created by Ana Mun on 17.09.2023.
//

#include "process.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Неверное число аргументов\n");
        return -1;
    }

    char *p;
    errno = 0;
    long num_of_child_proc = strtol(argv[2], &p, 10);

    if (errno != 0 || *p != '\0' || num_of_child_proc > MAX_PROCESS_ID || num_of_child_proc < 1) {
        printf("Непредусмотренное число процессов\n");
        return -1;
    } else {
        if (argc == 4 && strcmp(argv[3], "--mutexl") == 0) {
            start_parent(num_of_child_proc, 1);
        } else start_parent(num_of_child_proc, 0);
    }
    return 0;
}


