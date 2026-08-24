/*
 * file_sync.c — canonical C11 port of tools/environments/file_sync.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_environments_file_sync_u_resolve_host_path @ tools/environments/file_sync.py:_resolve_host_path */
int tools_environments_file_sync_u_resolve_host_path(const char *arg) {
    /* Python: first host whose remote == remote_path, else None. Arg =
     * "remote_path\thost=remote\thost=remote..." */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    size_t rlen = (size_t)(tab - arg);
    const char *p = tab + 1;
    while (*p) {
        const char *eq = strchr(p, '=');
        const char *t2 = strchr(p, '\t');
        if (eq && (!t2 || eq < t2)) {
            size_t remote_len = (t2 ? (size_t)(t2 - eq - 1) : strlen(eq + 1));
            if (remote_len == rlen && strncmp(eq + 1, arg, rlen) == 0) {
                printf("%.*s\n", (int)(eq - p), p);
                return 0;
            }
        }
        p = t2 ? t2 + 1 : p + strlen(p);
    }
    printf("\n");
    return 0;
}
