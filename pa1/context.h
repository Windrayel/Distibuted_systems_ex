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
};
