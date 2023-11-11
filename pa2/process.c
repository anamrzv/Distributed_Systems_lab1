//
// Created by Ana Mun on 17.09.2023.
//

#include "process.h"

int pipe_log_file;
int events_log_file;

long processes_num;

int start_parent(long children_num, const balance_t *balance) {
    open_log_files();

    processes_num = children_num + 1;
    int my_local_id = PARENT_ID;

    int pipe_write_ends[PROCESS_NUM][PROCESS_NUM] = {{0}};
    int pipe_read_ends[PROCESS_NUM][PROCESS_NUM] = {{0}};

    int open_result = open_all_pipe_ends(pipe_read_ends, pipe_write_ends);
    if (open_result == ERROR) return -1;

    for (int i = 1; i < processes_num; i++) {
        pid_t child_pid = fork();
        if (child_pid == -1) {
            printf("fork() failed for process number %d\n", i + 1);
            close_all_pipe_ends(pipe_read_ends, pipe_write_ends);
            return -1;
        } else if (child_pid == 0) { // child process
            my_local_id = i;
            timestamp_t start_time = get_physical_time();

            balance_t my_local_balance = balance[i - 1];
            BalanceHistory my_local_balance_history;
            my_local_balance_history.s_id = (local_id) my_local_id;
            my_local_balance_history.s_history_len = 0;

            //write start to log
            char buf[BUFFER_80];
            write_events_log(buf, snprintf(buf, 80, log_started_fmt, start_time, my_local_id, getpid(), getppid(),
                                           my_local_balance));

            //leave only its ends
            close_specific_pipe_ends(my_local_id, pipe_read_ends, pipe_write_ends);

            //send start
            Message *declaration = malloc(sizeof(Message));
            declaration->s_header.s_magic = MESSAGE_MAGIC;
            declaration->s_header.s_type = STARTED;
            declaration->s_header.s_local_time = start_time;
            int length = snprintf(declaration->s_payload, MAX_PAYLOAD_LEN, log_started_fmt, start_time,
                                  my_local_id, getpid(), getppid(), my_local_balance);
            declaration->s_header.s_payload_len = length;

            struct msg_transfer msg_tran = {my_local_id, pipe_write_ends[my_local_id], pipe_read_ends[my_local_id],
                                            processes_num};
            int start_result = send_multicast(&msg_tran, declaration);
            if (start_result < 0) return -1;

            //receive "starts" from others
            int wait_result = wait_for_messages_from_everybody(&msg_tran, STARTED);
            if (wait_result < 0) return -1;
            write_events_log(buf,
                             snprintf(buf, BUFFER_80, log_received_all_started_fmt, get_physical_time(), my_local_id));

            //receive transfer, stop
            int other_done_count = 0;
            int not_stopped = 1;
            TransferOrder *transferOrder;
            while (other_done_count != processes_num - 3 || not_stopped != 0) {
                receive_any(&msg_tran, declaration);
                update_history(&my_local_balance_history, my_local_balance);
                switch (declaration->s_header.s_type) {
                    case TRANSFER:
                        transferOrder = (TransferOrder *) declaration->s_payload;
                        if (transferOrder->s_src == my_local_id) {
                            my_local_balance -= transferOrder->s_amount;
                            int send_result = send(&msg_tran, transferOrder->s_dst, declaration);
                            if (send_result == 0) {
                                write_events_log(buf, snprintf(buf, 80, log_transfer_out_fmt, get_physical_time(),
                                                               transferOrder->s_src,
                                                               transferOrder->s_amount, transferOrder->s_dst));
                            }
                            else perror("send");
                        } else if (transferOrder->s_dst == my_local_id) {
                            my_local_balance += transferOrder->s_amount;
                            write_events_log(buf, snprintf(buf, 80, log_transfer_in_fmt, get_physical_time(),
                                                           transferOrder->s_dst,
                                                           transferOrder->s_amount, transferOrder->s_src));
                            Message acknowledgment;
                            acknowledgment.s_header.s_type = ACK;
                            acknowledgment.s_header.s_magic = MESSAGE_MAGIC;
                            acknowledgment.s_header.s_payload_len = 0;
                            acknowledgment.s_header.s_local_time = get_physical_time();
                            int send_result = send(&msg_tran, PARENT_ID, &acknowledgment);
                            if (send_result < 0) perror("send");
                        }
                        break;
                    case STOP:
                        not_stopped = 0;
                        declaration->s_header.s_type = DONE;
                        declaration->s_header.s_magic = MESSAGE_MAGIC;
                        declaration->s_header.s_local_time = get_physical_time();
                        length = snprintf(declaration->s_payload, MAX_PAYLOAD_LEN, log_done_fmt, declaration->s_header.s_local_time,
                                          my_local_id,
                                          my_local_balance);
                        declaration->s_header.s_payload_len = length;

                        int stop_result = send_multicast(&msg_tran, declaration);
                        write_events_log(buf,
                                         snprintf(buf, BUFFER_80, "%d sent stop to pipe write ends %d %d %d %d %d\n", my_local_id, pipe_write_ends[my_local_id][1], pipe_write_ends[my_local_id][2], pipe_write_ends[my_local_id][3], pipe_write_ends[my_local_id][4], pipe_write_ends[my_local_id][5]));
                        if (stop_result < 0) perror("send_multicast");
                        else write_events_log(buf,
                                              snprintf(buf, BUFFER_80, log_done_fmt, declaration->s_header.s_local_time, my_local_id,
                                                       my_local_balance));
                        break;
                    case DONE:
                        other_done_count++;
                        write_events_log(buf,
                                         snprintf(buf, BUFFER_80, "%d got done %s, other count - %d\n", my_local_id, declaration->s_payload, other_done_count));
                        break;
                    default:
                        perror("receive any");
                }
            }

            //got other done
            write_events_log(buf,snprintf(buf, BUFFER_80, log_received_all_done_fmt, get_physical_time(), my_local_id));

            my_local_balance_history.s_history[my_local_balance_history.s_history_len].s_time = my_local_balance_history.s_history_len;
            my_local_balance_history.s_history[my_local_balance_history.s_history_len].s_balance = my_local_balance;
            my_local_balance_history.s_history[my_local_balance_history.s_history_len].s_balance_pending_in = 0;
            my_local_balance_history.s_history_len++;

            declaration->s_header.s_type = BALANCE_HISTORY;
            declaration->s_header.s_local_time = get_physical_time();
            declaration->s_header.s_payload_len = sizeof(BalanceHistory);
            memcpy(declaration->s_payload, &my_local_balance_history, declaration->s_header.s_payload_len);
            send(&msg_tran, PARENT_ID, declaration);
            write_events_log(buf,snprintf(buf, BUFFER_80, "%d sent history\n", my_local_id));

            //close_left_pipe_ends(my_local_id, pipe_write_ends[my_local_id], pipe_read_ends[my_local_id]);
            //free(declaration);

            return 0;
        }
    }

    //leave only its ends
    close_specific_pipe_ends(my_local_id, pipe_read_ends, pipe_write_ends);

    //wait all children are started
    char buf[BUFFER_80];
    struct msg_transfer msg_tran = {my_local_id, pipe_write_ends[my_local_id], pipe_read_ends[my_local_id], processes_num};
    int wait_result = wait_for_messages_from_everybody(&msg_tran, STARTED);
    if (wait_result < 0) return ERROR;
    write_events_log(buf, snprintf(buf, BUFFER_80, log_received_all_started_fmt, get_physical_time(), my_local_id));

    //bank robbery
    bank_robbery(&msg_tran, (local_id) children_num);

    //send stop
    Message *declaration = malloc(sizeof(Message));
    declaration->s_header.s_magic = MESSAGE_MAGIC;
    declaration->s_header.s_type = STOP;
    declaration->s_header.s_local_time = get_physical_time();
    declaration->s_header.s_payload_len = 0;

    int send_result = send_multicast(&msg_tran, declaration);
    if (send_result < 0) return ERROR;

    //wait all children are done
    wait_result = wait_for_messages_from_everybody(&msg_tran, DONE);
    if (wait_result < 0) return ERROR;
    write_events_log(buf, snprintf(buf, BUFFER_80, log_received_all_done_fmt, get_physical_time(), my_local_id));

    //get and print history
    wait_for_history_from_everybody(&msg_tran);

    close_left_pipe_ends(my_local_id, pipe_write_ends[my_local_id], pipe_read_ends[my_local_id]);
    close_log_files();
    free(declaration);

    return 0;
}

void update_history(BalanceHistory* balance_history, balance_t cur_balance) {
    timestamp_t curr_time = get_physical_time();

    for (timestamp_t i = balance_history->s_history_len; i < curr_time; ++i) {
        BalanceState* balance_state = balance_history->s_history + i;
        balance_state->s_time = i;
        balance_state->s_balance = cur_balance;
        balance_state->s_balance_pending_in = 0;
    }
    balance_history->s_history_len = curr_time;
}

int wait_for_history_from_everybody(void *void_dest) {
    int received_declarations = 0;
    int received_process_nums[PROCESS_NUM] = {0};
    long waited_processes_num = processes_num - 1;
    Message *answer = malloc(sizeof(Message));
    BalanceHistory balance_history;
    AllHistory all_history;
    all_history.s_history_len = waited_processes_num;
    char buf[BUFFER_80];
    while (received_declarations != waited_processes_num) {
        for (int waited_proc_id = 1; waited_proc_id < processes_num; waited_proc_id++) {
            write_events_log(buf, snprintf(buf, BUFFER_80, "received declarations number: %d\n", received_declarations));
            if (received_declarations == waited_processes_num) break;
            if (received_process_nums[waited_proc_id] == 1) continue; //msg was already read
            int result = receive(void_dest, (local_id) waited_proc_id, answer);
            switch (result) {
                case SUCCESS:
                    if (answer->s_header.s_type == BALANCE_HISTORY) {
                        write_events_log(buf, snprintf(buf, BUFFER_80, "received history from %d\n", waited_proc_id));
                        received_declarations += 1;
                        received_process_nums[waited_proc_id] = 1;
                        memcpy((void *) &balance_history, answer->s_payload,
                               sizeof(char) * answer->s_header.s_payload_len);
                        all_history.s_history[waited_proc_id - 1] = balance_history;
                        break;
                    } else break;
                case EMPTY:
                    sleep(1);
                    break;
                case EMPTY_EOF:
                    break;
                default:
                    return ERROR;
            }
        }
    }
    print_history(&all_history);
    free(answer);
    return SUCCESS;
}

int wait_for_messages_from_everybody(void *void_dest, MessageType supposed_type) {
    struct msg_transfer *dest = (struct msg_transfer *) void_dest;
    int received_declarations = 0;
    int received_process_nums[PROCESS_NUM] = {0};
    long waited_processes_num;
    if (dest->id == 0) waited_processes_num = processes_num - 1;
    else waited_processes_num = processes_num - 2;
    Message *answer = malloc(sizeof(Message));
    while (received_declarations != waited_processes_num) {
        for (int waited_proc_id = 1; waited_proc_id < processes_num; waited_proc_id++) {
            if (waited_proc_id == dest->id) continue;
            if (received_process_nums[waited_proc_id] == 1) continue; //msg was already read
            int result = receive(void_dest, (local_id) waited_proc_id, answer);
            switch (result) {
                case SUCCESS:
                    if (answer->s_header.s_type == supposed_type) {
                        received_declarations += 1;
                        received_process_nums[waited_proc_id] = 1;
                    } else continue;
                case EMPTY:
                    sleep(1);
                    continue;
                case EMPTY_EOF:
                    continue;
                default:
                    return ERROR;
            }
        }
    }
    free(answer);
    return SUCCESS;
}

void close_left_pipe_ends(int process_id, int *pipe_write_ends, int *pipe_read_ends) {
    for (int second_proc_num = 0; second_proc_num < processes_num; second_proc_num++) {
        if (second_proc_num == process_id) continue;
        close(pipe_write_ends[second_proc_num]);
        write_pipe_log_close(process_id, second_proc_num, pipe_write_ends[second_proc_num], CLOSED_WRITE);

        close(pipe_read_ends[second_proc_num]);
        write_pipe_log_close(second_proc_num, process_id, pipe_read_ends[second_proc_num], CLOSED_READ);
    }
}

void close_all_pipe_ends(int pipe_read_ends[PROCESS_NUM][PROCESS_NUM], int pipe_write_ends[PROCESS_NUM][PROCESS_NUM]) {
    for (int i = 0; i < processes_num; i++) {
        for (int j = 0; j < processes_num; j++) {
            if (pipe_read_ends[i][j] != NOT_EXIST) {
                close(pipe_read_ends[i][j]);
                write_pipe_log_close(j, i, pipe_read_ends[i][j], CLOSED_READ);
            }
            if (pipe_write_ends[i][j] != NOT_EXIST) {
                close(pipe_write_ends[i][j]);
                write_pipe_log_close(i, j, pipe_write_ends[i][j], CLOSED_WRITE);
            }
        }
    }
}

void close_specific_pipe_ends(int process_id, int pipe_read_ends[PROCESS_NUM][PROCESS_NUM],
                              int pipe_write_ends[PROCESS_NUM][PROCESS_NUM]) {
    for (int from = 0; from < processes_num; from++) {
        for (int to = 0; to < processes_num; to++) {
            if (from == to) continue;

            if (from != process_id) {
                close(pipe_write_ends[from][to]);
                write_pipe_log_close(from, to, pipe_write_ends[from][to], CLOSED_WRITE);
                pipe_write_ends[from][to] = -777;

                close(pipe_read_ends[from][to]);
                write_pipe_log_close(from, to, pipe_read_ends[from][to], CLOSED_READ);
                pipe_read_ends[from][to] = -777;
            }
        }
    }
}

int open_all_pipe_ends(int pipe_read_ends[PROCESS_NUM][PROCESS_NUM], int pipe_write_ends[PROCESS_NUM][PROCESS_NUM]) {
    int fd[2];
    for (int from = 0; from < processes_num; from++) {
        for (int to = 0; to < processes_num; to++) {
            if (from != to) {
                if (pipe(fd) < 0) {
                    perror("pipe");
                    close_all_pipe_ends(pipe_read_ends, pipe_write_ends);
                    return ERROR;
                }
                write_pipe_log_open(from, to, fd[0], fd[1]);

                if (fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | O_NONBLOCK) < 0) {
                    perror("fcntl");
                    close_all_pipe_ends(pipe_read_ends, pipe_write_ends);
                    return ERROR;
                }

                pipe_read_ends[to][from] = fd[0];
                pipe_write_ends[from][to] = fd[1];
            } else {
                pipe_read_ends[to][from] = NOT_EXIST;
                pipe_write_ends[from][to] = NOT_EXIST;
            }
        }
    }
    return SUCCESS;
}

void write_events_log(const char *message, int message_len) {
    ssize_t bytes_written = write(events_log_file, message, message_len);
    printf("%s", message);
    if (bytes_written == -1) {
        printf("Couldn't write event log");
    }
}

void write_pipe_log_close(int first, int second, int fd, enum pipe_log_type type) {
    char message[100];
    char *pattern;
    int length;
    if (type == CLOSED_WRITE)
        pattern = "Pipe's WRITE end from %d to %d was CLOSED. Number %d\n";
    else if (type == CLOSED_READ)
        pattern = "Pipe's READ end to %d from %d was CLOSED. Number %d\n";

    length = snprintf(message, sizeof(message), pattern,
                      first,
                      second,
                      fd);

    ssize_t bytes_written = write(pipe_log_file, message, length);
    printf("%s", message);
    if (bytes_written == -1) {
        printf("Couldn't write log for closing pipe between processes with local ids %d and %d\n", first, second);
    }
}

void write_pipe_log_open(int first, int second, int fd0, int fd1) {
    char message[100];
    char *pattern = "Pipe between processes %d and %d was OPENED. Number read: %d write: %d\n";
    int length = snprintf(message, sizeof(message), pattern,
                          first,
                          second,
                          fd0, fd1);

    ssize_t bytes_written = write(pipe_log_file, message, length);
    printf("%s", message);
    if (bytes_written == -1) {
        printf("Couldn't write log for opening pipe between processes with local ids %d and %d\n", first, second);
    }
}

void open_log_files(void) {
    pipe_log_file = open(pipes_log, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (pipe_log_file == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    events_log_file = open(events_log, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (events_log_file == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
}

void close_log_files(void) {
    if (close(pipe_log_file) != 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }
    if (close(events_log_file) != 0) {
        perror("close()");
        exit(EXIT_FAILURE);
    }
}


