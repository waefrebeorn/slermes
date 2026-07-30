/* Oracle harness: gateway/drain_control.py vs LIVE Python.
 * Uses a temp HERMES_HOME so FS ops are isolated. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gateway/port_gateway_drain_control.h"

static const char *js(const char *s) {
    static char bufs[8][1024];
    static int bi = 0; char *b = bufs[bi]; bi = (bi + 1) % 8;
    char *q = b; *q++ = '"';
    for (const char *p = s; p && *p && q - b < 1000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

int main(void) {
    const char *home = getenv("SLERMES_HOME");
    /* clear any existing marker */
    gateway_drain_control_clear_drain_request(home);

    /* read when absent -> null */
    char *r0 = gateway_drain_control_read_drain_request(home);
    printf("{\"t\":\"read_absent\",\"out\":%s}\n", r0 ? js(r0) : "null");
    free(r0);

    /* write */
    char *w = gateway_drain_control_write_drain_request("drain-control", 0, home);
    printf("{\"t\":\"write\",\"out\":%s}\n", js(w));
    free(w);

    /* read back */
    char *r1 = gateway_drain_control_read_drain_request(home);
    printf("{\"t\":\"read_present\",\"out\":%s}\n", js(r1));
    free(r1);

    /* clear */
    bool cleared = gateway_drain_control_clear_drain_request(home);
    printf("{\"t\":\"clear\",\"out\":%d}\n", cleared?1:0);

    /* clear again -> false (idempotent) */
    bool cleared2 = gateway_drain_control_clear_drain_request(home);
    printf("{\"t\":\"clear_again\",\"out\":%d}\n", cleared2?1:0);
    return 0;
}
