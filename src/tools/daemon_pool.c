/*
 * daemon_pool.c — canonical C11 port of tools/daemon_pool.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_daemon_pool_u_adjust_thread_count @ tools/daemon_pool.py:_adjust_thread_count */
int tools_daemon_pool_u_adjust_thread_count(const char *arg) {
    /* Python: spawn daemon worker when under max. Arg =
     * "num_threads\tmax_workers\tspawned". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long num = strtol(arg, NULL, 10);
    long max = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    if (num < max) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}
