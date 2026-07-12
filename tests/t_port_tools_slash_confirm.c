/* Oracle harness: tools/slash_confirm.py vs LIVE Python. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tools/port_tools_slash_confirm.h"

static const char *js(const char *s) {
    static char bufs[8][512];
    static int bi = 0; char *b = bufs[bi]; bi = (bi + 1) % 8;
    char *q = b; *q++ = '"';
    for (const char *p = s; p && *p && q - b < 500; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; } else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

static char *fake_handler(const char *choice) {
    char *r = malloc(64); snprintf(r, 64, "handled:%s", choice); return r;
}

int main(void) {
    /* get_pending when none */
    char *p0 = tools_slash_confirm_get_pending("sess1");
    printf("{\"t\":\"get_none\",\"out\":%s}\n", p0 ? js(p0) : "null");
    if (p0) free(p0);

    /* register */
    tools_slash_confirm_register("sess1", "cid-1", "reload-mcp", fake_handler);
    char *p1 = tools_slash_confirm_get_pending("sess1");
    printf("{\"t\":\"get\",\"out\":%s}\n", p1 ? js(p1) : "null");
    if (p1) free(p1);

    /* resolve */
    char *res = tools_slash_confirm_resolve("sess1", "cid-1", "once", 300);
    printf("{\"t\":\"resolve\",\"out\":%s}\n", res ? js(res) : "null");
    if (res) free(res);

    /* after resolve, pending cleared -> get returns null */
    char *p2 = tools_slash_confirm_get_pending("sess1");
    printf("{\"t\":\"get_after_resolve\",\"out\":%s}\n", p2 ? js(p2) : "null");
    if (p2) free(p2);

    /* resolve with wrong confirm_id -> null */
    tools_slash_confirm_register("sess2", "cid-A", "reload-mcp", fake_handler);
    char *res2 = tools_slash_confirm_resolve("sess2", "cid-WRONG", "once", 300);
    printf("{\"t\":\"resolve_wrong\",\"out\":%s}\n", res2 ? js(res2) : "null");
    if (res2) free(res2);

    /* clear */
    tools_slash_confirm_clear("sess2");
    char *p3 = tools_slash_confirm_get_pending("sess2");
    printf("{\"t\":\"get_after_clear\",\"out\":%s}\n", p3 ? js(p3) : "null");
    if (p3) free(p3);
    return 0;
}
