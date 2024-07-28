#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "common.h"
#include "pa2345.h"
#include "context.h"

int n;
timestamp_t lamport_time = 0;

timestamp_t get_lamport_time() {
    return lamport_time;
}

void set_pipes(struct context * ctx, int read_pipes[n][n], int write_pipes[n][n]) {
    for (int i = 0; i < n; i++) {
        if (i != ctx->id) {
            ctx->read_pipes[i] = read_pipes[ctx->id][i];
            ctx->write_pipes[i] = write_pipes[i][ctx->id];
        }
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j)
                continue;
            if (i != ctx->id && j != ctx->id) {
                close(read_pipes[i][j]);
                close(write_pipes[i][j]);
            }
            if (i == ctx->id && j != ctx->id) {
                close(read_pipes[j][i]);
                close(write_pipes[i][j]);
            }
        }
    }
}

void set_message_header(Message* msg, MessageType type, timestamp_t timestamp) {
    MessageHeader msg_header = {
            .s_magic = MESSAGE_MAGIC,
            .s_type = type,
            .s_payload_len = strlen(msg->s_payload),
            .s_local_time = timestamp,
    };
    if (type == CS_REQUEST || type == CS_REPLY || type == CS_RELEASE)
        msg_header.s_payload_len = 0;
    msg->s_header = msg_header;
}

void print_log(char string[]) {
    FILE * file = fopen(events_log, "a");
    fprintf(file, "%s", string);
    printf("%s", string);
    fclose(file);
}

local_id min(const timestamp_t array[MAX_PROCESS_ID], int len) {
    timestamp_t timestamp = 32767;
    local_id thread_id = 0;

    for (int i = 1; i < len; ++i) {
        if (array[i] < timestamp) {
            timestamp = array[i];
            thread_id = i;
        }
    }

    return thread_id;
}

int request_cs(const void * self) {
    struct context * ctx = (struct context *) self;
    Message msg;

    lamport_time++;
    ctx->queue[ctx->id] = get_lamport_time();
    set_message_header(&msg, CS_REQUEST, get_lamport_time());
    send_multicast(ctx, &msg);
    return 0;
}

int release_cs(const void * self) {
    struct context * ctx = (struct context *) self;
    Message msg;

    ctx->queue[ctx->id] = 32767;
    lamport_time++;
    set_message_header(&msg, CS_RELEASE, get_lamport_time());
    send_multicast(ctx, &msg);
    return 0;
}

void process_messages(struct context * ctx, int * reply_count, int * done_msg_count) {
    Message msg;
    //// Receiving messages and waiting for critical section
    while (1) {
        while (receive_any(ctx, &msg)) {}
        if (msg.s_header.s_local_time > get_lamport_time()) {
            lamport_time = msg.s_header.s_local_time + 1;
        } else {
            lamport_time++;
        }
        switch (msg.s_header.s_type) {
            case CS_REQUEST:
                ctx->queue[last_from] = msg.s_header.s_local_time;

                lamport_time++;
                set_message_header(&msg, CS_REPLY, get_lamport_time());
                send(ctx, last_from, &msg);
                break;

            case CS_REPLY:
                *reply_count = *reply_count + 1;
                break;

            case CS_RELEASE:
                ctx->queue[last_from] = 32767;
                break;

            case DONE:
                *done_msg_count = *done_msg_count + 1;
                break;

            default:
                printf("error");
        }

//// Stop receiving messages and go to CS
        if ((*reply_count == ctx->n - 2 && min(ctx->queue, ctx->n) == ctx->id) || *done_msg_count == ctx->n - 2) {
            break;
        }
    }
}

int main(int argc, char * argv[]) {
    n = 9;
//    system("> events.log");
    for (int i = 0; i < 3; i++) {
        char mini_buf[20];
        sprintf(mini_buf, "%s\n", argv[i]);
        print(mini_buf);
    }

    int read_pipes[n][n];
    int write_pipes[n][n];

    memset(read_pipes, 0, sizeof(read_pipes));
    memset(write_pipes, 0, sizeof(write_pipes));

    FILE *pipes_file = fopen(pipes_log, "w");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j)
                continue;
            int fd[2];
            pipe(fd);
            fcntl(fd[0], F_SETFL, O_NONBLOCK);
            fcntl(fd[1], F_SETFL, O_NONBLOCK);
            read_pipes[i][j] = fd[0];
            write_pipes[i][j] = fd[1];
            fprintf(pipes_file, "%d / %d\n", fd[0], fd[1]);
        }
    }
    fclose(pipes_file);
    FILE *event_file = fopen(events_log, "a");
    fclose(event_file);

    struct context main_ctx = {
            .n = n,
            .id = 0,
            .pid = getpid(),
            .parent_pid = 0,
    };

    pid_t pids[n];
    struct context ctx;
    int fork_val = 0;

////Forking and children work
    for (int l = 1; l < n; l++) {
        fork_val = fork();

        if (fork_val > 0) {
            pids[l] = fork_val;
        } else {
            ctx.n = n;
            ctx.id = l;
            ctx.pid = getpid();
            ctx.parent_pid = main_ctx.pid;
            break;
        }

    }
    if (fork_val == 0) {
        for (int i = 0; i < ctx.n; i++) {
            ctx.queue[i] = 32767;
        }
        set_pipes(&ctx, read_pipes, write_pipes);
        Message msg;
        char buffer[2048];
        lamport_time++;

//// Write to buffer for messages and logs
        sprintf(buffer, log_started_fmt, get_lamport_time(), ctx.id, ctx.pid, ctx.parent_pid, 0);

//// Sending STARTED messages
        print_log(buffer);
        sprintf(msg.s_payload, "%s", buffer);
        set_message_header(&msg, STARTED, get_lamport_time());
        send_multicast(&ctx, &msg);

//// Receiving STARTED messages
        for (local_id i = 1; i < ctx.n; i++) {
            if (i == ctx.id) {
                continue;
            }
            while (receive(&ctx, i, &msg)) {}
            if (msg.s_header.s_local_time > get_lamport_time()) {
                lamport_time = msg.s_header.s_local_time + 1;
            } else {
                lamport_time++;
            }
        }

//// Logging received all Started
        sprintf(buffer, log_received_all_started_fmt, get_lamport_time(), ctx.id);
        print_log(buffer);

//// Main work
        int * done_msg_count = (int *) malloc(sizeof(int));
        int * reply_count = (int *) malloc(sizeof(int));
        *done_msg_count = 0;
        *reply_count = 0;

        for (int i = 1; i <= ctx.id * 5; i++) {
                request_cs(&ctx);
                process_messages(&ctx, reply_count, done_msg_count);
//// Critical section
                char sub_buf[64];
                snprintf(sub_buf, 64, log_loop_operation_fmt, ctx.id, i, ctx.id * 5);

                print(sub_buf);

                *reply_count = 0;
                release_cs(&ctx);
        }

//// Send DONE messages
        lamport_time++;
        sprintf(buffer, log_done_fmt, get_lamport_time(), ctx.id, 0);
        print_log(buffer);
        sprintf(msg.s_payload, "%s", buffer);
        set_message_header(&msg, DONE, get_lamport_time());
        send_multicast(&ctx, &msg);
        *reply_count = ctx.n - 2;

        process_messages(&ctx, reply_count, done_msg_count);

        free(done_msg_count);
        free(reply_count);

        sprintf(buffer, log_received_all_done_fmt, get_lamport_time(), ctx.id);
        print_log(buffer);
        exit(0);
    }

//// Main process work
    set_pipes(&main_ctx, read_pipes, write_pipes);
    Message msg;

//// Receiving STARTED messages
    for (int i = 1; i < n; i++) {
        while (receive_any(&main_ctx, &msg)) {}
        if (msg.s_header.s_local_time > get_lamport_time()) {
            lamport_time = msg.s_header.s_local_time + 1;
        } else {
            lamport_time++;
        }
    }

//// Receiving DONE messages
    for (int i = 0; i < n - 1;) {
        while (receive_any(&main_ctx, &msg)) {}
        if (msg.s_header.s_type == DONE)
            i++;
        if (msg.s_header.s_local_time > get_lamport_time()) {
            lamport_time = msg.s_header.s_local_time;
        } else {
            lamport_time++;
        }
    }

    for (int i = 1; i < n; ++i) {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}
