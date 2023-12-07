//
// Created by Ana Mun on 06.12.2023.
//

#ifndef DISTRIBUTED_SYSTEMS_LAB1_PRIORITY_QUEUE_H
#define DISTRIBUTED_SYSTEMS_LAB1_PRIORITY_QUEUE_H

#include <stdlib.h>
#include "ipc.h"

typedef struct node {
    local_id pid;
    timestamp_t ts;
    struct node* next;
} Node;

Node* new_node(local_id pid, timestamp_t ts);
Node* peek(Node** head);
void pop_by_pid(Node** head, local_id pid);
void push(Node** head, local_id pid, timestamp_t ts);

#endif //DISTRIBUTED_SYSTEMS_LAB1_PRIORITY_QUEUE_H
