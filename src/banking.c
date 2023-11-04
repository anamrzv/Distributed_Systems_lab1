//
// Created by Ana Mun on 04.11.2023.
//

#include "./include/banking.h"
#include "./include/process.h"

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
    char buf[BUFFER_80];
    int length;
    struct msg_transfer* msg_tran = (struct msg_transfer*) parent_data;

    Message *declaration = malloc(sizeof(Message));
    declaration->s_header.s_magic = MESSAGE_MAGIC;
    declaration->s_header.s_type = TRANSFER;
    declaration->s_header.s_local_time = get_physical_time();
    declaration->s_header.s_payload_len = sizeof(TransferOrder);
    TransferOrder transferOrder = {.s_src = src, .s_dst = dst, .s_amount = amount};
    memcpy(&transferOrder, &(declaration->s_payload), sizeof(TransferOrder));
    int send_result = send(&parent_data, src, declaration);
    if (send_result > 0) {
        printf(log_transfer_out_fmt, get_physical_time(), src, amount, dst);
        length = snprintf(buf, 80, log_transfer_out_fmt, get_physical_time(), src, amount, dst);
        write_events_log(buf, length);

        while (receive(&msg_tran, dst, declaration) < 0 || declaration->s_header.s_type != ACK);

        printf(log_transfer_out_fmt, get_physical_time(), src, amount, dst);
        length = snprintf(buf, 80, log_transfer_in_fmt, get_physical_time(), src, amount, dst);
        write_events_log(buf, length);
    } else {
        printf("transfer() failed: could not sent msg to source process %d\n", src);
        exit(1);
    }
}


//uint16_t length = snprintf(declaration->s_payload, MAX_PAYLOAD_LEN, log_transfer_out_fmt, declaration->s_header.s_local_time,
//                           src, amount, dst);