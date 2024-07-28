#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "context.h"
#include "common.h"
#include "pa2345.h"
#include "banking.h"

const int n = 6;

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
    if (type == ACK || type == STOP)
        msg_header.s_payload_len = 0;
    if (type == TRANSFER)
        msg_header.s_payload_len = sizeof(TransferOrder);
    if (type == BALANCE_HISTORY)
        msg_header.s_payload_len = sizeof(BalanceHistory);
    msg->s_header = msg_header;
}

void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount) {
    struct context * ctx = (struct context *)parent_data;
    Message msg;
    TransferOrder order  = {
            .s_src = src,
            .s_dst = dst,
            .s_amount = amount,
    };
    memcpy(msg.s_payload, &order, sizeof(TransferOrder));
    set_message_header(&msg, TRANSFER, get_physical_time());
    send(ctx, src, &msg);
    while (receive(ctx, order.s_dst, &msg)){}
}

void print_log(char string[]) {
    FILE * file = fopen(events_log, "a");
    fprintf(file, "%s", string);
    printf("%s", string);
    fclose(file);

}

int main(int argc, char * argv[]) {

    //bank_robbery(parent_data);
    //print_history(all);
//    n = atoi(argv[2]);
//    n++;
//    system("> events.log");
    int balance[argc-3];
    for (int i = 0; i < argc - 3; i++) {
        balance[i] = atoi(argv[i+3]);
    }

    int read_pipes[n][n];
    int write_pipes[n][n];

    FILE * pipes_file = fopen(pipes_log, "w");
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
    FILE * event_file = fopen(events_log, "a");
    fclose(event_file);

    struct context main_ctx = {
            .n = n,
            .id = 0,
            .pid = getpid(),
            .parent_pid = 0,
    };

    pid_t pids[n];

    ////Forking and children work
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
                .balance = balance[i-1]
        };
        set_pipes(&ctx, read_pipes, write_pipes);

        Message msg;
        char buffer[2048];

        sprintf(buffer, log_started_fmt, get_physical_time(), ctx.id, ctx.pid, ctx.parent_pid, ctx.balance);
        sprintf(msg.s_payload, "%s", buffer);
        print_log(buffer);

        set_message_header(&msg, STARTED, get_physical_time());
        send(&ctx, 0, &msg);

        BalanceHistory bh = {
                .s_id = ctx.id,
                .s_history_len = 1,
        };

        BalanceState bs = {
                .s_balance = ctx.balance,
                .s_time = get_physical_time(),
                .s_balance_pending_in = 0,
        };
        bh.s_history[0] = bs;

        while (1) {
            int break_flag = 0;
            timestamp_t timestamp;

            while (receive_any(&ctx, &msg)) {}
            TransferOrder * order;

            switch (msg.s_header.s_type) {
                case STOP:
                    break_flag = 1;
                    break;

                case TRANSFER:
                    order = (TransferOrder*) msg.s_payload;
                    if (order->s_src == ctx.id) {
                        ctx.balance -= order->s_amount;
                        timestamp = get_physical_time();
                        sprintf(buffer, log_transfer_out_fmt, timestamp, ctx.id, order->s_amount, order->s_dst);
                        print_log(buffer);

                        set_message_header(&msg, TRANSFER, get_physical_time());
                        send(&ctx, order->s_dst, &msg);

                    } else {
                        ctx.balance += order->s_amount;
                        timestamp = get_physical_time();
                        sprintf(buffer, log_transfer_in_fmt, timestamp, ctx.id, order->s_amount, order->s_src);
                        print_log(buffer);

                        set_message_header(&msg, ACK, get_physical_time());
                        send(&ctx, 0, &msg);
                    }
                    bs.s_balance = ctx.balance;
                    bs.s_time = timestamp;
                    bh.s_history[timestamp] = bs;
                    bh.s_history_len = timestamp + 1;
            }
            if (break_flag) {
                break;
            }
        }
        sprintf(msg.s_payload, log_done_fmt, get_physical_time(), ctx.id, ctx.balance);
        set_message_header(&msg, DONE, get_physical_time());
        send(&ctx, 0, &msg);

        sprintf(buffer, log_done_fmt, get_physical_time(), ctx.id, ctx.balance);
        print_log(buffer);

        memcpy(msg.s_payload, &bh, sizeof(BalanceHistory) - sizeof(BalanceState) * (MAX_PROCESS_ID - bh.s_history_len));
        set_message_header(&msg, BALANCE_HISTORY, get_physical_time());
        send(&ctx, 0, &msg);

        exit(0);
    }
    set_pipes(&main_ctx, read_pipes, write_pipes);

    Message msg;

    for (int i = 1; i < n; i++) {
        while (receive_any(&main_ctx, &msg)) {}
    }

    bank_robbery(&main_ctx, n - 1);

    set_message_header(&msg, STOP, get_physical_time());
    send_multicast(&main_ctx, &msg);

    //// Receiving DONE messages
    for (int i = 1; i < n; i++) {
        while (receive(&main_ctx, i, &msg)) {}
    }

    AllHistory allHistory = {
            .s_history_len = n - 1,
    };

    timestamp_t last = 0;
    for (int i = 1; i < n; i++) {
        while (receive(&main_ctx, i, &msg)) {}
        BalanceHistory * bh = (BalanceHistory*) msg.s_payload;
        if (bh->s_history[bh->s_history_len - 1].s_time > last) {
            last = bh->s_history[bh->s_history_len - 1].s_time;
        }

        allHistory.s_history[i-1] = *bh;
    }

    for (int i = 0; i < n - 1; ++i) {
        allHistory.s_history[i].s_history_len = last + 1;
        balance_t previous = allHistory.s_history[i].s_history[0].s_balance;
        for (int j = 0; j <= last; ++j) {
            if (allHistory.s_history[i].s_history[j].s_time != j) {
                allHistory.s_history[i].s_history[j].s_time = j;
                allHistory.s_history[i].s_history[j].s_balance = previous;
            } else {
                previous = allHistory.s_history[i].s_history[j].s_balance;
            }
        }

    }

    print_history(&allHistory);

    for (int i = 1; i < n; ++i) {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}
