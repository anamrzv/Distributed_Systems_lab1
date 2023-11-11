//
// Created by Ana Mun on 04.11.2023.
//

#include "banking.h"
#include "process.h"

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
    struct msg_transfer* msg_tran = (struct msg_transfer*) parent_data;

    Message *declaration = malloc(sizeof(Message));
    declaration->s_header.s_magic = MESSAGE_MAGIC;
    declaration->s_header.s_type = TRANSFER;
    declaration->s_header.s_local_time = get_physical_time();
    declaration->s_header.s_payload_len = sizeof(TransferOrder);
    TransferOrder transferOrder = {.s_src = src, .s_dst = dst, .s_amount = amount};
    memcpy(&(declaration->s_payload), &transferOrder, sizeof(TransferOrder));
    int send_result = send(parent_data, src, declaration);
    if (send_result > ERROR) {
        while (receive(msg_tran, dst, declaration) != 0 || declaration->s_header.s_type != ACK);
    } else {
        perror("send");
        printf("transfer() failed: could not sent msg to source process %d\n", src);
        exit(1);
    }
}
