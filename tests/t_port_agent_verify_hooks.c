/* Oracle harness: agent/verify_hooks.py vs LIVE Python. */
#include <stdio.h>
#include "agent/port_agent_verify_hooks.h"

static const char *js(const char *s) {
    static char bufs[8][8192];
    static int bi = 0; char *b = bufs[bi]; bi = (bi + 1) % 8;
    char *q = b; *q++ = '"';
    for (const char *p = s; p && *p && q - b < 8000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

int main(void) {
    /* max_verify_nudges */
    printf("{\"t\":\"nudges\",\"cfg\":%s,\"out\":%d}\n", js("null"), agent_verify_hooks_max_verify_nudges(NULL));
    printf("{\"t\":\"nudges\",\"cfg\":%s,\"out\":%d}\n", js("{\"agent\":{\"max_verify_nudges\": 5}}"),
           agent_verify_hooks_max_verify_nudges("{\"agent\":{\"max_verify_nudges\": 5}}"));
    printf("{\"t\":\"nudges\",\"cfg\":%s,\"out\":%d}\n", js("{\"agent\":{\"max_verify_nudges\": -2}}"),
           agent_verify_hooks_max_verify_nudges("{\"agent\":{\"max_verify_nudges\": -2}}"));
    printf("{\"t\":\"nudges\",\"cfg\":%s,\"out\":%d}\n", js("{\"agent\":{\"max_verify_nudges\": \"abc\"}}"),
           agent_verify_hooks_max_verify_nudges("{\"agent\":{\"max_verify_nudges\": \"abc\"}}"));

    /* coding_verify_guidance */
    const char *g1 = agent_verify_hooks_coding_verify_guidance(NULL);
    printf("{\"t\":\"guidance\",\"cfg\":%s,\"out\":%s}\n", js("null"), g1 ? js(g1) : "null");
    const char *g2 = agent_verify_hooks_coding_verify_guidance("{\"agent\":{\"verify_guidance\": false}}");
    printf("{\"t\":\"guidance\",\"cfg\":%s,\"out\":%s}\n", js("{\"agent\":{\"verify_guidance\": false}}"), g2 ? js(g2) : "null");
    const char *g3 = agent_verify_hooks_coding_verify_guidance("{\"agent\":{\"verify_guidance\": \"yes\"}}");
    printf("{\"t\":\"guidance\",\"cfg\":%s,\"out\":%s}\n", js("{\"agent\":{\"verify_guidance\": \"yes\"}}"), g3 ? js(g3) : "null");
    return 0;
}
