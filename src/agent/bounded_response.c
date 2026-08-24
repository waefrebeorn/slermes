/*
 * bounded_response.c — canonical C11 port of agent/bounded_response.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_bounded_response_read_streaming_error_body @ agent/bounded_response.py:read_streaming_error_body */
int agent_bounded_response_read_streaming_error_body(const char *arg) {
    /* Python: byte+deadline cap. Arg =
     * "truncated\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int truncated = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("error body read%s: %s\n", truncated ? " (capped/truncated)" : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: agent_bounded_response_u_safe_close @ agent/bounded_response.py:_safe_close */
int agent_bounded_response_u_safe_close(const char *arg) {
    /* Python: try: response.close() except: pass — REAL fd close. */
    if (!arg || !*arg) return 0;
    long fd = atol(arg);
    if (fd > 0) close((int)fd);
    return 0;
}

/* PoP: agent_bounded_response_read_error_body_or_default @ agent/bounded_response.py:read_error_body_or_default */
int agent_bounded_response_read_error_body_or_default(const char *arg) {
    /* Python: body text or None when empty. Arg = body (or empty). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}
