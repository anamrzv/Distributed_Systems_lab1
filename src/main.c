//
// Created by Ana Mun on 17.09.2023.
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "include/process.h"
#include "include/ipc.h"

int main(int argc, char *argv[]) {

    char *p;

    errno = 0;
    long conv = strtol(argv[1], &p, 10);

    if (errno != 0 || *p != '\0' || conv > MAX_PROCESS_ID || conv < 1) {
        printf("Invalid argument: must be between 1 and 10\n");
        return -1;
    } else {
        start_parent(conv);
    }
}
