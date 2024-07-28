#include "ipc.h"
#include "context.h"
#include <unistd.h>

int send(void * self, local_id dst, const Message * msg) {
    struct context * ctx = (struct context *)self;
    if (write(ctx->write_pipes[dst], &msg->s_header, sizeof(MessageHeader)) <= 0)
        return 1;
    write(ctx->write_pipes[dst], &msg->s_payload, msg->s_header.s_payload_len);
    return 0;
}

int send_multicast(void * self, const Message * msg) {
    struct context * ctx = (struct context *)self;

    for (local_id id = 0; id < ctx->n; ++id) {
        if (id == ctx->id)
            continue;
        if (send(ctx, id, msg) != 0)
            return 1;
    }

    return 0;
}

int receive(void * self, local_id from, Message * msg) {
    struct context * ctx = (struct context *)self;
    int cur_read = read(ctx->read_pipes[from], &msg->s_header, sizeof(MessageHeader));
    int read_len = 0;
    if (cur_read <= 0) {
        return 1;
    }
    while (read_len < sizeof(MessageHeader)) {
        if (cur_read > 0) {
            read_len += cur_read;
        }
        cur_read += read(ctx->read_pipes[from], &msg->s_header + read_len, sizeof(MessageHeader) - read_len);
    }
    read_len = 0;
    while (read_len < msg->s_header.s_payload_len) {
        cur_read = read(ctx->read_pipes[from], &msg->s_payload + read_len, msg->s_header.s_payload_len - read_len);
        if (cur_read > 0) {
            read_len += cur_read;
        }
    }
    return 0;
}

int receive_any(void * self, Message * msg) {
    struct context * ctx = (struct context *)self;

    for (local_id id = 0; id < ctx->n; ++id) {
        if (id == ctx->id)
            continue;
        if (receive(ctx, id, msg) == 0) {
            return 0;
        }
    }

    return 1;
}
