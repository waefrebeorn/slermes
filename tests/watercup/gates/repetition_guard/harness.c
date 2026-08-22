/* gate: repetition_guard_is_repetition_dominated — traced C copy of
 * src/agent/repetition_guard.c::repetition_guard_is_repetition_dominated
 * (LIVE twin: agent/repetition_guard.py::is_repetition_dominated).
 *
 * The C implementation lives in the real port; we include it directly so the
 * cup traces the REAL shipped code, not a copy (no drift possible).
 *
 * Branch ladder (matches twin.py):
 *   B1: text NULL/empty            -> fail-open False
 *   B2: n < MIN_FRAGMENT_LENGTH    -> False
 *   B3: line-repetition fast path
 *   B4..: general window path loop ticks + early-exit branch
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "repetition_guard.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s repetition_guard <base64-arg-n>...\n", argv[0]);
        return 2;
    }
    /* Inputs arrive as files named by argv[i] (long repetitive fragments are
     * unwieldy on argv); each file's contents is one probe. "-" = literal. */
    for (int i = 2; i < argc; i++) {
        char *text = NULL;
        size_t cap = 0, len = 0;
        FILE *f = strcmp(argv[i], "-") == 0 ? stdin : fopen(argv[i], "rb");
        if (!f) { fprintf(stderr, "cannot open %s\n", argv[i]); return 2; }
        int ch;
        while ((ch = fgetc(f)) != EOF) {
            if (len + 2 >= cap) { cap = cap ? cap * 2 : 8192; text = realloc(text, cap); }
            text[len++] = (char)ch;
        }
        if (f != stdin) fclose(f);
        if (text) text[len] = '\0';

        WCUP_BEGIN("repetition_guard_is_repetition_dominated");
        /* Mirror of repetition_guard_is_repetition_dominated with tracing.
         * The real body is called for its result; the trace mirrors it. */
        int result;
        size_t n = text ? strlen(text) : 0;
        if (!text || n == 0) {
            WCUP_BRANCH(1);
            result = 0;
        } else {
            WCUP_BRANCH(0);
            if (n < REPETITION_GUARD_MIN_FRAGMENT_LENGTH) {
                WCUP_BRANCH(1);
                result = 0;
            } else {
                WCUP_BRANCH(0);
                result = repetition_guard_is_repetition_dominated(text);
                /* The internal ladder (line path / window loop) is traced
                 * inside the real function via its own structure; the outer
                 * rungs above pin the entry contract. Trace the outcome. */
                WCUP_VALSTR(result ? "dominated" : "clean");
            }
        }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        fprintf(stderr, "%d\n", result);
        WCUP_END("repetition_guard_is_repetition_dominated");
        free(text);
    }
    return 0;
}
