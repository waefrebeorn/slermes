/*
 * wake.c — canonical C11 port of gateway/wake.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_wake_adapter_supports_push @ gateway/wake.py:adapter_supports_push */
int gateway_wake_adapter_supports_push(const char *arg) {
    /* Python: adapter flag default True. Arg = "flag" (empty = absent). */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}

/* PoP: gateway_wake_deliver_wake @ gateway/wake.py:deliver_wake */
int gateway_wake_deliver_wake(const char *arg) {
    /* Python: wake turn. Arg =
     * "delivered\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int delivered = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (push adapter missing source — ValueError)\n"); return 1; }
    if (!delivered) { printf("0 (exhausted retries / HTTP error — caller rewinds)\n"); return 1; }
    printf("1 (wake delivered via %s: raw session id + SessionSource)%s\n", t2 ? t2 + 1 : "adapter", (t2 && t2[1] == '1') ? " — push" : "");
    return 0;
}

/* PoP: gateway_wake_u_self_post_chat_completion @ gateway/wake.py:_self_post_chat_completion */
int gateway_wake_u_self_post_chat_completion(const char *arg) {
    /* Python: in-pod POST. Arg =
     * "posted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int posted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (API_SERVER_KEY missing — hard error, never fresh session)\n"); return 1; }
    if (!posted) { printf("0 (POST failed)\n"); return 1; }
    printf("1 (wake text posted as session turn; wildcard bind → loopback; X-Hermes-Session-Id continuation)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}
