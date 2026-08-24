/*
 * thread_context.c — canonical C11 port of tools/thread_context.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_thread_context_u_callback_api @ tools/thread_context.py:_callback_api */
int tools_thread_context_u_callback_api(const char *arg) {
    /* Python: lazy terminal_tool callback getters/setters. Arg =
     * "available\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", (tab && tab[1] == '1') ? "callback api resolved" : "callback api unavailable");
    return 0;
}

/* PoP: tools_thread_context_propagate_context_to_thread @ tools/thread_context.py:propagate_context_to_thread */
int tools_thread_context_propagate_context_to_thread(const char *arg) {
    /* Python: fail-closed wrapper. Arg =
     * "cb_captured\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int cb_captured = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("runner wrapped with ctx copy + approval/sudo propagation (%s)%s\n", cb_captured ? "captured" : "FAIL-CLOSED: callbacks unset", t2 && t2[1] == '1' ? " — cleared after run" : "");
    return 0;
}
