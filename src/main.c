//
// Created by Ana Mun on 17.09.2023.
//

#include <getopt.h>
#include "include/process.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Неверное число аргументов\n");
        return -1;
    }

    const char *shortOptions = "p:";
    char *num_of_processes = 0;
    struct option longOptions[] = {
            {"process number", 1, 0,    'p'},
            {NULL,             0, NULL, 0}
    };

    int rez;
    int opIdx;
    while ((rez = getopt_long(argc, argv, shortOptions, longOptions, &opIdx)) != -1) {
        switch (rez) {
            case 'p': {
                num_of_processes = optarg;
                break;
            }
            case '?': {
                printf("Неизвестная опция\n");
                break;
            }
            default:
                printf("Опций не обнаружено\n");
                break;
        }
    }

    char *p;
    long conv = strtol(num_of_processes, &p, 10);
    if (conv < 1 || conv > MAX_PROCESS_ID) {
        printf("Непредусмотренное число процессов\n");
        return -1;
    }
    start_parent(conv);

}
