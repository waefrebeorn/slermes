/* gate: gw_format_truncate_message — traced copy of
 * src/gateway/gw_format.c::gw_truncate_message (E43).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: text empty / max_len == 0     -> NULL input guard
 *   B2: len <= max_len                -> verbatim copy
 *   B3: word-boundary break_at found and break_at > max_len/2 -> soft cut
 *   B4: else                          -> hard cut at max_len
 *   V1: resulting string
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static char *gw_truncate_message(const char *text, size_t max_len, char *out, size_t outsz) {
    WCUP_BEGIN("gw_truncate_message");
    /* B1: guard */
    if (!text || max_len == 0 || text[0] == '\0') {
        WCUP_BRANCH(1);
        out[0] = '\0';
        WCUP_VALSTR(out);
        goto done;
    }
    WCUP_BRANCH(0);
    {
        size_t len = strlen(text);
        if (len <= max_len) {
            WCUP_BRANCH(1);
            snprintf(out, outsz, "%s", text);
            WCUP_VALSTR(out);
            goto done;
        }
        WCUP_BRANCH(0);

        char buf[4096];
        memcpy(buf, text, max_len);

        int break_at = (int)max_len;
        while (break_at > 0 && buf[break_at - 1] != ' ') break_at--;

        if (break_at > (int)max_len / 2) {
            WCUP_BRANCH(1);
            buf[break_at] = '\0';
            snprintf(out, outsz, "%s...", buf);
        } else {
            WCUP_BRANCH(0);
            buf[max_len] = '\0';
            snprintf(out, outsz, "%s...", buf);
        }
        WCUP_VALSTR(out);
    }
done:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("gw_truncate_message");
        return out;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_truncate_message <text|maxlen>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char probe[4096];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *bar = strrchr(probe, '|');
        size_t maxlen = bar ? (size_t)atoi(bar + 1) : 0;
        if (bar) *bar = '\0';
        char out[8192];
        gw_truncate_message(probe, maxlen, out, sizeof out);
    }
    return 0;
}
