//
// Created by Ana Mun on 17.09.2023.
//

#include "process.h"
#include "priority_queue.h"

int pipe_log_file;
int events_log_file;

long processes_num;
timestamp_t my_current_timestamp;
Node* my_priority_queue = NULL;
extern local_id last_sender_id;
int received_done = 0;

int start_parent(long children_num, int mutex_mode) {
    open_log_files();

    my_current_timestamp = 0;
    processes_num = children_num + 1;
    local_id my_local_id = PARENT_ID;

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
            my_local_id = (local_id ) i;
            my_current_timestamp++;
            timestamp_t start_time = get_lamport_time();

            //write start to log
            char buf[BUFFER_80];
            write_events_log(buf,snprintf(buf, BUFFER_80, log_started_fmt, start_time, my_local_id, getpid(), getppid(), 0));

            //leave only its ends
            //close_specific_pipe_ends(my_local_id, pipe_read_ends, pipe_write_ends);

            //send start
            Message *declaration = malloc(sizeof(Message));
            declaration->s_header.s_magic = MESSAGE_MAGIC;
            declaration->s_header.s_type = STARTED;
            declaration->s_header.s_local_time = start_time;
            int length = snprintf(declaration->s_payload, MAX_PAYLOAD_LEN, log_started_fmt, start_time, my_local_id,
                                  getpid(), getppid(), 0);
            declaration->s_header.s_payload_len = length;
            struct msg_transfer msg_tran = {my_local_id, pipe_write_ends[my_local_id], pipe_read_ends[my_local_id],
                                            processes_num};
            int start_result = send_multicast(&msg_tran, declaration);
            if (start_result < 0) return -1;

            //receive "starts" from others
            int wait_result = wait_for_messages_from_everybody(&msg_tran, STARTED); //updated time
            if (wait_result < 0) return -1;
            write_events_log(buf,snprintf(buf, BUFFER_80, log_received_all_started_fmt, get_lamport_time(), my_local_id));

            //critical section
            for (int jobs = 1; jobs <= my_local_id * 5; jobs++) {
                snprintf(buf, BUFFER_80, log_loop_operation_fmt, my_local_id, jobs, my_local_id * 5);
                if (mutex_mode) {
                    request_cs(&msg_tran);
                    print(buf);
                    release_cs(&msg_tran);
                } else print(buf);
            }

            //send done
            my_current_timestamp++;
            declaration->s_header.s_type = DONE;
            declaration->s_header.s_local_time = get_lamport_time();
            length = snprintf(declaration->s_payload, MAX_PAYLOAD_LEN, log_done_fmt, get_lamport_time(), my_local_id, 0);
            declaration->s_header.s_payload_len = length;
            int done_result = send_multicast(&msg_tran, declaration);
            if (done_result < 0) return -1;

            //got other done
            while (received_done != processes_num - 2) {
                receive_any(&msg_tran, declaration);
                calc_timestamp(declaration->s_header.s_local_time, my_current_timestamp);
                switch (declaration->s_header.s_type) {
                    case CS_REQUEST:
                        printf("process %d received request from %d\n", my_local_id, last_sender_id);
                        push(&my_priority_queue, last_sender_id, declaration->s_header.s_local_time);
                        declaration->s_header.s_local_time = get_lamport_time();
                        declaration->s_header.s_type = CS_REPLY;
                        send(&msg_tran, last_sender_id, declaration);
                        break;
                    case CS_RELEASE:
                        break;
                    case DONE:
                        received_done++;
                        break;
                }
            }
            write_events_log(buf, snprintf(buf, BUFFER_80, log_received_all_done_fmt, get_lamport_time(), my_local_id));

            //close_left_pipe_ends(my_local_id, pipe_write_ends[my_local_id], pipe_read_ends[my_local_id]);
            free(declaration);
            return 0;
        }
    }

    //leave only its ends
    //close_specific_pipe_ends(my_local_id, pipe_read_ends, pipe_write_ends);

    //wait all children are started
    char buf[BUFFER_80];
    struct msg_transfer msg_tran = {my_local_id, pipe_write_ends[my_local_id], pipe_read_ends[my_local_id], processes_num};
    int wait_result = wait_for_messages_from_everybody(&msg_tran, STARTED);
    if (wait_result < 0) return ERROR;
    write_events_log(buf, snprintf(buf, BUFFER_80, log_received_all_started_fmt, get_lamport_time(), my_local_id));

    //wait all children are done
    wait_result = wait_for_messages_from_everybody(&msg_tran, DONE);
    if (wait_result < 0) return ERROR;
    write_events_log(buf, snprintf(buf, BUFFER_80, log_received_all_done_fmt, get_lamport_time(), my_local_id));

    while (wait(NULL) > 0);

    close_left_pipe_ends(my_local_id, pipe_write_ends[my_local_id], pipe_read_ends[my_local_id]);
    close_log_files();

    return 0;
}

int request_cs(const void* void_transfer_struct) {
    struct msg_transfer *transfer_struct = (struct msg_transfer *) void_transfer_struct;

    my_current_timestamp++;
    Message *declaration = malloc(sizeof(Message));
    declaration->s_header.s_magic = MESSAGE_MAGIC;
    declaration->s_header.s_type = CS_REQUEST;
    declaration->s_header.s_local_time = get_lamport_time();
    declaration->s_header.s_payload_len = 0;
    int result = send_multicast(transfer_struct, declaration);
    if (result < 0) return ERROR;
    printf("process %d send request\n", transfer_struct->id);

    push(&my_priority_queue, transfer_struct->id, get_lamport_time());
    printf("process %d pushed\n", transfer_struct->id);

    int replies = 0;
    while (replies != processes_num - 2 || peek(&my_priority_queue)->pid != transfer_struct->id) {
        receive_any(transfer_struct, declaration);
        calc_timestamp(declaration->s_header.s_local_time, my_current_timestamp);
        switch (declaration->s_header.s_type) {
            case CS_REQUEST:
                printf("process %d received request from %d\n", transfer_struct->id, last_sender_id);
                push(&my_priority_queue, last_sender_id, declaration->s_header.s_local_time);
                declaration->s_header.s_local_time = get_lamport_time();
                declaration->s_header.s_type = CS_REPLY;
                result = send(transfer_struct, last_sender_id, declaration);
                if (result < 0) return ERROR;
                break;
            case CS_REPLY:
                printf("process %d received reply from %d\n", transfer_struct->id, last_sender_id);
                replies++;
                break;
            case CS_RELEASE:
                printf("process %d received release from %d. top was %d\n", transfer_struct->id, last_sender_id, peek(&my_priority_queue)->pid);
                pop_by_pid(&my_priority_queue, last_sender_id);
                printf("process %d popped, top is %d, replies are %d\n", transfer_struct->id, peek(&my_priority_queue)->pid, replies);
                break;
            case DONE:
                printf("process %d received done from %d\n", transfer_struct->id, last_sender_id);
                received_done++;
                break;
        }
    }
    free(declaration);
    return SUCCESS;
}

int release_cs(const void* void_transfer_struct) {
    struct msg_transfer *transfer_struct = (struct msg_transfer *) void_transfer_struct;

    pop_by_pid(&my_priority_queue, transfer_struct->id);

    my_current_timestamp++;
    Message *declaration = malloc(sizeof(Message));
    declaration->s_header.s_magic = MESSAGE_MAGIC;
    declaration->s_header.s_type = CS_RELEASE;
    declaration->s_header.s_local_time = get_lamport_time();
    declaration->s_header.s_payload_len = 0;
    int result = send_multicast(transfer_struct, declaration);
    if (result < 0) {
        printf("AAAAA\n");
        return ERROR;
    }
    printf("%d send RELEASE\n", transfer_struct->id);
    free(declaration);
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
                        calc_timestamp(answer->s_header.s_local_time, my_current_timestamp);
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

                if (fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | O_NONBLOCK) < 0 ||
                    fcntl(fd[1], F_SETFL, fcntl(fd[1], F_GETFL) | O_NONBLOCK) < 0) {
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
        pattern = "Process %d CLOSED Pipe's WRITE end to %d (Pipe Number %d)\n";
    else if (type == CLOSED_READ)
        pattern = "Process %d CLOSED Pipe's READ end to %d (Pipe Number %d)\n";

    length = snprintf(message, sizeof(message), pattern,
                      first,
                      second,
                      fd);

    ssize_t bytes_written = write(pipe_log_file, message, length);
    //printf("%s", message);
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

timestamp_t get_lamport_time(void) {
    return my_current_timestamp;
}

void calc_timestamp(timestamp_t external_timestamp, timestamp_t internal_counter) {
    if (external_timestamp > internal_counter) my_current_timestamp = (timestamp_t) (external_timestamp + 1);
    else my_current_timestamp = (timestamp_t) (internal_counter + 1);
}

