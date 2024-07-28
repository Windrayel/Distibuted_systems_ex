#include "ipc.h"
#include <sys/types.h>
#include <stdio.h>

struct context {
    int n;
    local_id id;
    pid_t pid;
    pid_t parent_pid;
    int read_pipes[15];
    int write_pipes[15];
    timestamp_t my_request;

//// Array index for id, value for bool
    int plan_to_send[15];
};

local_id last_from;
