/* gate: approval_mask_quoted_newlines — traced C copy of
 * src/tools/approval_helpers2.c::apr2_mask_quoted_newlines.
 * Faithful port of LIVE Python tools/approval.py::_mask_quoted_newlines.
 * WCUP_* records the SAME control structure so the cup matches iff the C port
 * executes the identical branch ladder as LIVE Python.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

static char *apr2_mask_quoted_newlines(const char *command) {
    WCUP_BEGIN("mask_quoted_newlines");
    char *out;
    if (!strchr(command, '\n')) {
        WCUP_BRANCH(0);
        out = strdup(command);
        goto done;
    }
    WCUP_BRANCH(1);
    size_t cap = strlen(command) + 1;
    out = malloc(cap);
    size_t k = 0;
    int quote = 0; /* 0 none, '\'' single, '"' double */
    size_t len = strlen(command);
    size_t i = 0;
    while (i < len) {
        WCUP_LOOP_TICK();
        char ch = command[i];
        if (quote) {
            WCUP_BRANCH(1);
            int esc = (quote == '"' && ch == '\\' && i + 1 < len);
            WCUP_BRANCH(esc);
            if (esc) {
                out[k++] = ch; out[k++] = command[i + 1];
                i += 2;
                continue;
            }
            WCUP_BRANCH(ch == quote);
            if (ch == quote) quote = 0;
            out[k++] = (ch == '\n') ? ' ' : ch;
            i++;
            continue;
        }
        WCUP_BRANCH(0);
        int is_q = (ch == '\'' || ch == '"');
        WCUP_BRANCH(is_q);
        if (is_q) {
            quote = ch;
        } else {
            int bs = (ch == '\\' && i + 1 < len);
            WCUP_BRANCH(bs);
            if (bs) {
                out[k++] = ch; out[k++] = command[i + 1];
                i += 2;
                continue;
            }
        }
        out[k++] = ch;
        i++;
    }
    out[k] = '\0';
done:
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mask_quoted_newlines");
    return out;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr2_mask_quoted_newlines <command>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) free(apr2_mask_quoted_newlines(argv[i]));
    return 0;
}
