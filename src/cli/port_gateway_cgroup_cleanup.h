#ifndef PORT_GATEWAY_CGROUP_CLEANUP_H
#define PORT_GATEWAY_CGROUP_CLEANUP_H

#include <stddef.h>

/* C port of gateway/cgroup_cleanup.py. */
char *cgroup_own_path(void);
int  *cgroup_read_pids(const char *cgroup_path, int *out_count);
int   cgroup_reap(const char *cgroup_path);

#endif /* PORT_GATEWAY_CGROUP_CLEANUP_H */
