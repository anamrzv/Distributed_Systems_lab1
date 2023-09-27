//
// Created by Ana Mun on 17.09.2023.
//


#include "include/ipc.h"
#include "include/process.h"
#include "include/common.h"


int start_parent(long children_num) {
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

    long processes_num = children_num + 1;
    int my_local_id = PARENT_ID;
    my_current_timestamp = 0;

    int pipe_write_ends[PROCESS_NUM][PROCESS_NUM] = {0};
    int pipe_read_ends[PROCESS_NUM][PROCESS_NUM] = {0};

    int fd[2]; // todo: check position

    // open all pipes
    for (int from = 0; from < processes_num; from++) {
        for (int to = 0; to < processes_num; to++) {
            if (from != to) {
                if (pipe(fd) < 0) {
                    fprintf(stderr, "Creation of pipe failed.\n"); // todo: close pipes
                    return -1;
                }
                my_current_timestamp++;
                write_pipe_log(from, to, PARENT_ID, OPENED);

                if (fcntl(fd[0], F_SETFL, O_NONBLOCK) < 0) {
                    perror("fcntl");
                }
                pipe_read_ends[to][from] = fd[0];
                pipe_write_ends[from][to] = fd[1];
            } else {
                pipe_read_ends[to][from] = -999;
                pipe_write_ends[from][to] = -999;
            }
        }
    }

    //
//    my_current_timestamp++;
//    Message* declaration = malloc(sizeof (Message));
//    declaration->s_header.s_magic = MESSAGE_MAGIC;
//    declaration->s_header.s_type = STARTED;
//    declaration->s_header.s_local_time = my_current_timestamp;
//    int length = snprintf(declaration->s_payload, MAX_PAYLOAD_LEN, log_started_fmt,
//                          my_local_id, getpid(), getppid());
//    declaration->s_header.s_payload_len = length;
//
//    struct msg_source src = {my_local_id, pipe_write_ends[my_local_id], processes_num };
//    send_multicast(&src, declaration);
    //

    for (int i = 1; i < processes_num; i++) {
        pid_t child_pid = fork();
        my_current_timestamp++;
        if (child_pid == -1) {
            printf("fork() failed for process number %d\n", i + 1); // todo: close pipes
            return -1;
        } else if (child_pid > 0) {
            printf("%" PRId16 " | Child process %d is forked\n", my_current_timestamp, i);
        } else if (child_pid == 0) { // we are in child process
            //leave only its ends
            my_current_timestamp = 0;
            int current_child_num = i;
            my_local_id = i;
            for (int from = 0; from < processes_num; from++) {
                for (int to = 0; to < processes_num; to++) {
                    if (from == to) continue;

                    if (from != current_child_num) {
                        close(pipe_write_ends[from][to]);
                        my_current_timestamp++;
                        write_pipe_log(from, to, my_local_id, CLOSED_WRITE);

                        close(pipe_read_ends[from][to]);
                        my_current_timestamp++;
                        write_pipe_log(from, to, my_local_id, CLOSED_READ);
                    }
                }
            }

            //send start
            my_current_timestamp++;
            Message* declaration = malloc(sizeof (Message));
            declaration->s_header.s_magic = MESSAGE_MAGIC;
            declaration->s_header.s_type = STARTED;
            declaration->s_header.s_local_time = my_current_timestamp;
            int length = snprintf(declaration->s_payload, MAX_PAYLOAD_LEN, log_started_fmt,
                                  my_local_id, getpid(), getppid());
            declaration->s_header.s_payload_len = length;

            struct msg_source src = {my_local_id, pipe_write_ends[my_local_id], processes_num };
            send_multicast(&src, declaration);


            //todo: wait for all
            Message* answer = malloc(sizeof (Message));
            struct msg_destination dst = {my_local_id, pipe_read_ends[my_local_id], processes_num };
            receive_any(&dst, answer);

            free(answer);
            free(declaration);

            //todo: send end
            return 0;
        }
    }

    if (my_local_id == PARENT_ID) {
        for (int from = 0; from < processes_num; from++) {
            for (int to = 0; to < processes_num; to++) {
                if (from == to) continue;

                if (from != 0) {
                    close(pipe_write_ends[from][to]);
                    my_current_timestamp++;
                    write_pipe_log(from, to, my_local_id, CLOSED_WRITE);

                    close(pipe_read_ends[from][to]);
                    my_current_timestamp++;
                    write_pipe_log(from, to, my_local_id, CLOSED_READ);
                }
            }
        }

        int p = -1;
        wait(&p);
    }

    //todo: wait for children

    return 0;
}

int send_multicast(void* void_source, const Message* msg) {
    struct msg_source* source = (struct msg_source*) void_source;
    write_events_log(msg->s_payload, msg->s_header.s_payload_len);
    for (int i = 0; i < source->processes_num; i++) {
        if (i == source->id) continue;
        ssize_t a = write(source->write_ends[i], msg, sizeof(Message));
        if (a < 0) perror("write");
        else printf("%" PRId16 " | (send_multicast) to %d success | %s\n", my_current_timestamp, i, msg->s_payload);
        close(source->write_ends[i]); //todo: move to separate func
    }
    return 0; //todo: add error code
}

int receive_any(void* void_dest, Message* msg) {
    struct msg_destination* dest = (struct msg_destination*) void_dest;
    int received_declarations = 0;
    long waited_processes_num = dest->processes_num - 2;
    while (received_declarations != waited_processes_num) {
        for (int waited_proc_id = 1; waited_proc_id < dest->processes_num; waited_proc_id++) {
            if (waited_proc_id == dest->id) continue;
            if (fcntl(dest->read_ends[waited_proc_id], F_GETFD) < 0) continue;
            long read_result = read(dest->read_ends[waited_proc_id], msg, sizeof(Message));
            switch (read_result) {
                case -1: // case -1 means pipe is empty and errno = EAGAIN
                    if (errno == EAGAIN) {
                        printf("%d tries to receive: pipe from process %d is empty\n", dest->id, waited_proc_id);
                        sleep(1);
                        break;
                    } else {
                        perror("read");
                        exit(4);
                    }
                case 0: // case 0 means all bytes are read and EOF
                    printf("%d tries to receive: End of conversation with %d\n", dest->id, waited_proc_id);
                    close(dest->read_ends[waited_proc_id]);
                    received_declarations++;
                    break;
                default:
                    printf("%d received MSG = %s\n", dest->id, msg->s_payload);
            }
        }
    }
    return 0;
}

int send(void* void_source, local_id dst, const Message* msg) {
    struct msg_source* source = (struct msg_source*) void_source;
    write_events_log(msg->s_payload, msg->s_header.s_payload_len);
    printf("%" PRId16 " | %s", my_current_timestamp, msg->s_payload);
    write(source->write_ends[dst], msg, sizeof(Message));
    return 0; //todo: add error code
}

int calc_timestamp(int external_timestamp, int internal_counter) {
    if (external_timestamp > internal_counter) return external_timestamp + 1;
    else return internal_counter + 1;
}

void write_events_log(const char* message, int message_len) {
    ssize_t bytes_written = write(events_log_file, message, message_len);
    if (bytes_written == -1) {
        printf("Couldn't write event log");
    }
}

void write_pipe_log(int first, int second, int my_local_id, enum pipe_log_type type) {
    char message[100];
    if (type == OPENED)
        snprintf(message, sizeof(message), "%d | Process %d: Pipe between processes with local ids %d and %d was OPENED\n",
                 my_current_timestamp,
                 my_local_id, first,
                 second);
    else if (type == CLOSED_WRITE)
        snprintf(message, sizeof(message), "%d | Process %d: Pipe's end to WRITE from %d to %d was CLOSED\n",
                 my_current_timestamp, my_local_id,
                 first,
                 second);
    else if (type == CLOSED_READ)
        snprintf(message, sizeof(message), "%d | Process %d: Pipe's end to READ from %d to %d was CLOSED\n",
                 my_current_timestamp, my_local_id,
                 first,
                 second);
    size_t message_length = strlen(message);
    ssize_t bytes_written = write(pipe_log_file, message, message_length);
    if (bytes_written == -1) {
        printf("Couldn't write log for pipe between processes with local ids %d and %d\n", first, second);
    }
}
