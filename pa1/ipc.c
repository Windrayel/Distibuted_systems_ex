#include "ipc.h"
#include "context.h"
#include <unistd.h>

int send(void * self, local_id dst, const Message * msg) {
    struct context * ctx = self;
    int bytes = 0;

    bytes += write(ctx->write_pipes[dst], &msg->s_header, sizeof(MessageHeader));
    bytes += write(ctx->write_pipes[dst], &msg->s_payload, msg->s_header.s_payload_len);

    if (bytes)
        return 1;
    else
        return 0;
}

int send_multicast(void * self, const Message * msg) {
    struct context * ctx = self;
    int error = 0;

    for (local_id id = 0; id < ctx->n; ++id) {
        if (id == ctx->id)
            continue;
        error += send(ctx, id, msg);
    }

    return error;
}

int receive(void * self, local_id from, Message * msg) {
    struct context * ctx = self;
    int bytes = 0;

    bytes += read(ctx->read_pipes[from], &msg->s_header, sizeof(MessageHeader));
    bytes += read(ctx->read_pipes[from], &msg->s_payload, msg->s_header.s_payload_len);

    if (bytes)
        return 1;
    else
        return 0;
}

int receive_any(void * self, Message * msg) {
    struct context * ctx = self;

    for (local_id id = 0; id < ctx->n; ++id) {
        if (id == ctx->id)
            continue;
        if (receive(ctx, id, msg) == 0) {
            return 0;
        }
    }

    return 1;
}
