/* gate: input_sanitize_collapse_repeated_input_artifacts — traced C copy of
 * src/hermes_cli/input_sanitize.c::input_sanitize_collapse_repeated_input_artifacts
 * (LIVE twin: hermes_cli/input_sanitize.py::collapse_repeated_input_artifacts).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: empty text
 *   marker loop (L ticks): B2: text[index-mlen..] == marker?
 *   B3: repeat_count < min_repeats -> return unchanged
 *   B4: start>=2 and text[start-2:start] == "[e"
 *   B5: start>=1 and text[start-1] == '['
 *   V1: result prefix
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

static const char *MARKER = "~[[e";

static void collapse(const char *text, int min_repeats) {
    WCUP_BEGIN("collapse_repeated_input_artifacts");
    char out[4096];
    size_t mlen = strlen(MARKER);
    if (!text || !*text || mlen == 0) {
        WCUP_BRANCH(1);                                  /* B1 */
        snprintf(out, sizeof out, "%s", text ? text : "");
        WCUP_VALSTR(out);                                /* V1 */
        goto fin;
    }
    WCUP_BRANCH(0);
    {
        long index = (long)strlen(text);
        int repeat_count = 0;
        while (index >= (long)mlen &&
               strncmp(text + index - mlen, MARKER, mlen) == 0) {
            WCUP_LOOP_TICK();
            WCUP_BRANCH(1);                              /* B2 */
            repeat_count++;
            index -= (long)mlen;
        }
        WCUP_BRANCH(0);                                  /* B2 exit */
        if (repeat_count < min_repeats) {
            WCUP_BRANCH(1);                              /* B3 */
            snprintf(out, sizeof out, "%s", text);
            WCUP_VALSTR(out);                            /* V1 */
            goto fin;
        }
        WCUP_BRANCH(0);
        long start = index;
        if (start >= 2 && strncmp(text + start - 2, "[e", 2) == 0) {
            WCUP_BRANCH(1);                              /* B4 */
            start -= 2;
        } else {
            WCUP_BRANCH(0);
            if (start >= 1 && text[start - 1] == '[') {
                WCUP_BRANCH(1);                          /* B5 */
                start -= 1;
            } else {
                WCUP_BRANCH(0);
            }
        }
        if (start < 0) start = 0;
        memcpy(out, text, (size_t)start);
        out[start] = '\0';
        WCUP_VALSTR(out);                                /* V1 */
    }
fin:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("collapse_repeated_input_artifacts");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s collapse <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) collapse(argv[i], 4 /* min_repeats */);
    return 0;
}
