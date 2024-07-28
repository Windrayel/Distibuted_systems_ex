#include "ipc.h"
#include <sys/types.h>
#include <stdio.h>

struct context {
    int n;
    local_id id;
    pid_t pid;
    pid_t parent_pid;
    int read_pipes[MAX_PROCESS_ID];
    int write_pipes[MAX_PROCESS_ID];
    int is_reply_sent[MAX_PROCESS_ID];

//// Array index for id, value for timestamp
    timestamp_t queue[MAX_PROCESS_ID];
};

local_id last_from;
