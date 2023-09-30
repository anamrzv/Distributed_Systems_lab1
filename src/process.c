//
// Created by Ana Mun on 17.09.2023.
//

#include "include/process.h"

int pipe_log_file;
int events_log_file;

timestamp_t my_current_timestamp;
long processes_num;

int start_parent(long children_num) {
    open_log_files();

    my_current_timestamp = 0;
    processes_num = children_num + 1;
    int my_local_id = PARENT_ID;

    int pipe_write_ends[PROCESS_NUM][PROCESS_NUM] = {0};
    int pipe_read_ends[PROCESS_NUM][PROCESS_NUM] = {0};

    int open_result = open_all_pipe_ends(pipe_read_ends, pipe_write_ends);
    if (open_result == ERROR) return -1;

    for (int i = 1; i < processes_num; i++) {
        pid_t child_pid = fork();
        my_current_timestamp++;
        if (child_pid == -1) {
            printf("fork() failed for process number %d\n", i + 1);
            close_all_pipe_ends(pipe_read_ends, pipe_write_ends);
            return -1;
        } else if (child_pid == 0) { // child process
            my_current_timestamp = 0;
            my_local_id = i;

            //write start to log
            char buf[BUFFER_80];
            int length = snprintf(buf, 80, log_started_fmt, my_local_id, getpid(), getppid());
            write_events_log(buf, length);
            printf("%s", buf);

            //leave only its ends
            close_specific_pipe_ends(my_local_id, pipe_read_ends, pipe_write_ends);

            //send start
            Message* declaration = malloc(sizeof (Message));
            declaration->s_header.s_magic = MESSAGE_MAGIC;
            declaration->s_header.s_type = STARTED;
            declaration->s_header.s_local_time = my_current_timestamp;
            length = snprintf(declaration->s_payload, MAX_PAYLOAD_LEN, log_started_fmt,
                              my_local_id, getpid(), getppid());
            declaration->s_header.s_payload_len = length;

            struct msg_source src = {my_local_id, pipe_write_ends[my_local_id], processes_num};
            int start_result = send_multicast(&src, declaration);
            if (start_result < 0) return -1;
            my_current_timestamp++;

            //receive "starts" from others
            struct msg_destination dst = {my_local_id, pipe_read_ends[my_local_id], processes_num };
            int wait_result = wait_for_messages_from_everybody(&dst, STARTED);
            if (wait_result < 0) return -1;
            my_current_timestamp++;
            length = snprintf(buf, BUFFER_80, log_received_all_started_fmt, my_local_id);
            write_events_log(buf, length);
            printf("%s", buf);

            //send done
            declaration->s_header.s_type = DONE;
            declaration->s_header.s_local_time = my_current_timestamp;
            length = snprintf(declaration->s_payload, MAX_PAYLOAD_LEN, log_done_fmt, my_local_id);
            declaration->s_header.s_payload_len = length;

            start_result = send_multicast(&src, declaration);
            if (start_result < 0) return -1;
            my_current_timestamp++;

            //receive "done" from others
            wait_result = wait_for_messages_from_everybody(&dst, DONE);
            if (wait_result < 0) return -1;
            my_current_timestamp++;
            length = snprintf(buf, BUFFER_80, log_received_all_done_fmt, my_local_id);
            write_events_log(buf, length);
            printf("%s", buf);

            close_left_pipe_ends(my_local_id, pipe_write_ends[my_local_id], pipe_read_ends[my_local_id]);
            free(declaration);

            return 0;
        }
    }

    //leave only its ends
    close_specific_pipe_ends(my_local_id, pipe_read_ends, pipe_write_ends);

    char buf[BUFFER_80];
    struct msg_destination dst = {my_local_id, pipe_read_ends[my_local_id], processes_num};
    int wait_result = wait_for_messages_from_everybody(&dst, STARTED);
    if (wait_result < 0) return ERROR;
    my_current_timestamp++;
    int length = snprintf(buf, BUFFER_80, log_received_all_started_fmt, my_local_id);
    write_events_log(buf, length);
    printf("%s", buf);

    wait_result = wait_for_messages_from_everybody(&dst, DONE);
    if (wait_result < 0) return ERROR;
    close_left_pipe_ends(my_local_id, pipe_write_ends[my_local_id], pipe_read_ends[my_local_id]);
    my_current_timestamp++;
    length = snprintf(buf, BUFFER_80, log_received_all_done_fmt, my_local_id);
    write_events_log(buf, length);
    printf("%s", buf);

    int status;
    for (size_t i = 0; i < children_num; i++) {
        wait(&status);
    }

    close_log_files();

    return 0;
}

int wait_for_messages_from_everybody(void* void_dest, MessageType supposed_type) {
    struct msg_destination* dest = (struct msg_destination*) void_dest;
    int received_declarations = 0;
    int received_process_nums[PROCESS_NUM] = {0};
    long waited_processes_num;
    if (dest->id == 0) waited_processes_num = processes_num - 1;
    else waited_processes_num = processes_num - 2;
    Message* answer = malloc(sizeof (Message));
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
                        my_current_timestamp = calc_timestamp(answer->s_header.s_local_time, my_current_timestamp);
                        my_current_timestamp++;
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
    return SUCCESS;
}

void close_left_pipe_ends(int process_id, int* pipe_write_ends,  int* pipe_read_ends) {
    for (int second_proc_num = 0; second_proc_num < processes_num; second_proc_num++) {
        if (second_proc_num == process_id) continue;
        close(pipe_write_ends[second_proc_num]);
        my_current_timestamp++;
        write_pipe_log_close(process_id, second_proc_num, pipe_write_ends[second_proc_num],  CLOSED_WRITE);

        close(pipe_read_ends[second_proc_num]);
        my_current_timestamp++;
        write_pipe_log_close(second_proc_num, process_id,pipe_read_ends[second_proc_num], CLOSED_READ);
    }
}

void close_all_pipe_ends(int pipe_read_ends[PROCESS_NUM][PROCESS_NUM], int pipe_write_ends[PROCESS_NUM][PROCESS_NUM]) {
    for (int i = 0; i < processes_num; i++) {
        for (int j = 0; j< processes_num; j++) {
            if (pipe_read_ends[i][j] != NOT_EXIST && fcntl(pipe_read_ends[i][j], F_GETFD) != -1) {
                close(pipe_read_ends[i][j]);
                my_current_timestamp++;
                write_pipe_log_close(j, i, pipe_read_ends[i][j], CLOSED_READ);
            }
            if (pipe_write_ends[i][j] != NOT_EXIST && fcntl(pipe_write_ends[i][j], F_GETFD) != -1) {
                close(pipe_write_ends[i][j]);
                my_current_timestamp++;
                write_pipe_log_close(i, j, pipe_write_ends[i][j],  CLOSED_WRITE);
            }
        }
    }
}

void close_specific_pipe_ends(int process_id, int pipe_read_ends[PROCESS_NUM][PROCESS_NUM], int pipe_write_ends[PROCESS_NUM][PROCESS_NUM]) {
    for (int from = 0; from < processes_num; from++) {
        for (int to = 0; to < processes_num; to++) {
            if (from == to) continue;

            if (from != process_id) {
                close(pipe_write_ends[from][to]);
                my_current_timestamp++;
                write_pipe_log_close(from, to, pipe_write_ends[from][to], CLOSED_WRITE);

                close(pipe_read_ends[from][to]);
                my_current_timestamp++;
                write_pipe_log_close(from, to, pipe_read_ends[from][to], CLOSED_READ);
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
                my_current_timestamp++;
                write_pipe_log_open(from, to, fd[0], fd[1]);

                if (fcntl(fd[0], F_SETFL, O_NONBLOCK) < 0) {
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

timestamp_t calc_timestamp(timestamp_t external_timestamp, timestamp_t internal_counter) {
    if (external_timestamp > internal_counter) return external_timestamp;
    else return internal_counter;
}

void write_events_log(const char* message, int message_len) {
    ssize_t bytes_written = write(events_log_file, message, message_len);
    if (bytes_written == -1) {
        printf("Couldn't write event log");
    }
}

void write_pipe_log_close(int first, int second, int fd, enum pipe_log_type type) {
    char message[100];
    char* pattern;
    int length;
    if (type == CLOSED_WRITE)
        pattern = "%d | Pipe's WRITE end from %d to %d was CLOSED. Number %d\n";
    else if (type == CLOSED_READ)
        pattern = "%d | Pipe's READ end to %d from %d was CLOSED. Number %d\n";

    length = snprintf(message, sizeof(message), pattern,
                      my_current_timestamp,
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
    char* pattern = "%d | Pipe between processes %d and %d was OPENED. Number read: %d write: %d\n";
    int length = snprintf(message, sizeof(message), pattern,
                          my_current_timestamp,
                          first,
                          second,
                          fd0, fd1);

    ssize_t bytes_written = write(pipe_log_file, message, length);
    printf("%s", message);
    if (bytes_written == -1) {
        printf("Couldn't write log for opening pipe between processes with local ids %d and %d\n", first, second);
    }
}

void open_log_files() {
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

void close_log_files() {
    if (close(pipe_log_file) != 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }
    if (close(events_log_file) != 0) {
        perror("close()");
        exit(EXIT_FAILURE);
    }
}


