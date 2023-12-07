//
// Created by Ana Mun on 06.12.2023.
//

#include "priority_queue.h"
#include <stdio.h>

Node* new_node(local_id pid, timestamp_t ts) {
    Node* temp = (Node*) malloc(sizeof(Node));
    temp->pid = pid;
    temp->ts = ts;
    temp->next = NULL;
    return temp;
}

Node* peek(Node** head) { return *head; }

void pop_by_pid(Node** head, local_id pid) {
    Node* start = *head;
    if (start->pid == pid) {
        (*head) = start->next;
        free(start);
    } else {
        while (start->next != NULL && start->next->pid != pid) {
            start = start->next;
        }
        Node* to_delete = start->next;
        start->next = start->next->next;
        free(to_delete);
    }
}

void push(Node** head, local_id pid, timestamp_t ts) {
    Node* start = *head;
    Node* temp = new_node(pid, ts);

    if (start == NULL) (*head) = temp;
    // The head of list has lesser priority than new node
    else if ( start->ts > ts || ((start->ts == ts) && start->pid > pid) ) {
        // Insert New Node before head
        temp->next = *head;
        (*head) = temp;
    } else {
        // Traverse the list and find a position to insert new node
        while (start->next != NULL && (start->next->ts < ts || (start->next->ts == ts && start->next->pid < pid) )) {
            start = start->next;
        }
        // Either at the ends of the list or at required position
        temp->next = start->next;
        start->next = temp;
    }
}
