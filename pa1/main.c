#include <stdlib.h>
#include <unistd.h>
#include "context.h"
#include "common.h"
#include "pa1.h"
#include <string.h>
#include <sys/wait.h>

int n;

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

void set_message(Message* msg, MessageType type, const char * const format, struct context * ctx) {
    sprintf(msg->s_payload, format, ctx->id, ctx->pid, ctx->parent_pid);
    MessageHeader msg_header = {
            .s_magic = MESSAGE_MAGIC,
            .s_type = type,
            .s_payload_len = strlen(msg->s_payload)
    };
    msg->s_header = msg_header;
}

void print_log(const char * format, int id, int pid, int parent_pid) {
    FILE * file = fopen(events_log, "a");
    fprintf(file, format, id, pid, parent_pid);
    printf(format, id, pid, parent_pid);
    fclose(file);
}

int main(int argc, char* argv[]) {
    n = atoi(argv[2]);
    n++;
//    system("> events.log");

    int read_pipes[n][n];
    int write_pipes[n][n];

    FILE * pipes_file = fopen(pipes_log, "w");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j)
                continue;
            int fd[2];
            pipe(fd);
            read_pipes[i][j] = fd[0];
            write_pipes[i][j] = fd[1];
            fprintf(pipes_file, "%d / %d\n", fd[0], fd[1]);
        }
    }
    fclose(pipes_file);
    FILE * event_file = fopen(events_log, "a");
    fclose(event_file);

    struct context main_ctx = {
            .n = n,
            .id = 0,
            .pid = getpid(),
            .parent_pid = 0,
    };
    for (int i = 0; i < n; i++) {
        if (i != main_ctx.id) {
            main_ctx.write_pipes[i] = write_pipes[main_ctx.id][i];
            main_ctx.read_pipes[i] = read_pipes[main_ctx.id][i];
        }
    }


    pid_t pids[n];

    for (int i = 1; i < n; i++) {
        int fork_val = fork();
        if (fork_val > 0) {
            pids[i] = fork_val;
            continue;
        }
        struct context ctx = {
                .n = n,
                .id = i,
                .pid = getpid(),
                .parent_pid = main_ctx.pid,
        };
        set_pipes(&ctx, read_pipes, write_pipes);

        Message msg;

        print_log(log_started_fmt, ctx.id, ctx.pid, ctx.parent_pid);

        set_message(&msg, STARTED, log_started_fmt, &ctx);
        send_multicast(&ctx, &msg);

        for (local_id id = 1; id < n; ++id) {
            if (id == ctx.id)
                continue;
            receive(&ctx, id, &msg);
        }
        print_log(log_received_all_started_fmt, ctx.id, ctx.pid, ctx.parent_pid);
        print_log(log_done_fmt, ctx.id, ctx.pid, ctx.parent_pid);

        set_message(&msg, DONE, log_done_fmt, &ctx);
        send_multicast(&ctx, &msg);

        for (local_id id = 1; id < n; ++id) {
            if (id == ctx.id) {
                continue;
            }
            receive(&ctx, id, &msg);
        }

        print_log(log_received_all_done_fmt, ctx.id, ctx.pid, ctx.parent_pid);

        exit(0);
    }


    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j)
                continue;
            if (i != main_ctx.id && j != main_ctx.id) {
                close(read_pipes[i][j]);
                close(write_pipes[i][j]);
            }
            if (i == main_ctx.id && j != main_ctx.id) {
                close(read_pipes[j][i]);
                close(write_pipes[i][j]);
            }
        }
    }

    Message msg;

    for (local_id id = 1; id < n; ++id) {
        receive(&main_ctx, id, &msg);
    }

    for (local_id id = 1; id < n; ++id) {
        receive(&main_ctx, id, &msg);
    }

    for (int i = 1; i < n; ++i) {
        waitpid(pids[i], NULL, 0);
    }

//    for (int i = 0; i < n; i++) {
//        for (int j = 0; j < n; j++) {
//            close(read_pipes[i][j]);
//            close(write_pipes[i][j]);
//        }
//    }
    return 0;
}
