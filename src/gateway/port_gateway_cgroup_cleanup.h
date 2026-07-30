#ifndef GATEWAY_CGROUP_CLEANUP_H
#define GATEWAY_CGROUP_CLEANUP_H
#include <stddef.h>
/* Returns 1 and fills out (>=1024) with the extracted path, or 0 when none. */
int gateway_cgroup_cleanup_own_cgroup_path(const char *buf, char *out, size_t outsz);
#endif
