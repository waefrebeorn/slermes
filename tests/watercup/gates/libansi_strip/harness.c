/* gate: libansi_strip_ansi — drives the REAL lib/libansi/ansi_strip.c
 * strip state machine. LIVE twin: tools/ansi_strip.py::strip_ansi.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: fast path — no ESC/C1 escape bytes present -> copy verbatim
 *   B2: slow path taken (escape sequences stripped)
 *   V1: resulting clean string
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
#include "ansi_strip.h"

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_strip_ansi <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *text = argv[i];
        WCUP_BEGIN("strip_ansi");
        char buf[4096];
        if (!ansi_has_escape(text)) {          /* B1 fast path */
            WCUP_BRANCH(1);
            snprintf(buf, sizeof buf, "%s", text ? text : "");
        } else {                               /* B2 slow path */
            WCUP_BRANCH(0);
            char *clean = strip_ansi(text);
            snprintf(buf, sizeof buf, "%s", clean ? clean : "");
            free(clean);
        }
        WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("strip_ansi");
    }
    return 0;
}
