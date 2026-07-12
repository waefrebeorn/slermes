/* Oracle harness: agent/turn_context.py _compression_made_progress vs LIVE Python. */
#include <stdio.h>
#include "agent/port_agent_turn_context.h"

static const char *js(const char *s) {
    static char bufs[8][256];
    static int bi = 0; char *b = bufs[bi]; bi = (bi + 1) % 8;
    char *q = b; *q++ = '"';
    for (const char *p = s; p && *p && q - b < 250; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; } else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

int main(void) {
    /* cases: (orig_len, new_len, orig_tokens, new_tokens, expected) */
    long cases[][4] = {
        {220, 220, 288000, 183000},  /* token reduction >5% -> True */
        {220, 220, 288000, 285000},  /* <5% -> False */
        {220, 100, 288000, 100000},  /* len reduction -> True */
        {220, 220, 0, 0},            /* no tokens -> False */
        {10, 10, 1000, 940},         /* exactly 6% reduction -> True */
    };
    for (int i = 0; i < 5; i++) {
        bool got = agent_turn_context_compression_made_progress(cases[i][0], cases[i][1], cases[i][2], cases[i][3]);
        printf("{\"t\":\"prog\",\"o\":%ld,\"n\":%ld,\"ot\":%ld,\"nt\":%ld,\"out\":%d}\n",
               cases[i][0], cases[i][1], cases[i][2], cases[i][3], got?1:0);
    }
    (void)js;
    return 0;
}
