/* gate: transcription_tools_shell_quote_context_stt — traced copy of
 * src/tools/transcription_wrappers.c::tsc_shell_quote_context_stt
 * (REWRITTEN to match LIVE). LIVE twin:
 * tools/transcription_tools.py::_shell_quote_context_stt.
 *
 * Same ladder as the tts gate: B1 null / B2 single / B3 double+escape /
 * B4-B6 open + bare-backslash skip.
 * Input encoding: "<template>|<position>" (split on LAST '|').
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static char *tsc_shell_quote_context_stt(const char *command, long position) {
    WCUP_BEGIN("shell_quote_context_stt");
    char result = 0;
    if (!command || position < 0) {          /* B1 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    {
        char quote = 0;
        int escaped = 0;
        long i = 0;
        while (i < position && command[i]) {
            WCUP_LOOP_TICK();
            char c = command[i];
            if (quote == '\'') {             /* B2 */
                WCUP_BRANCH(1);
                if (c == '\'') { WCUP_BRANCH(1); quote = 0; }
                else WCUP_BRANCH(0);
            } else if (quote == '"') {       /* B3 */
                WCUP_BRANCH(0);
                WCUP_BRANCH(1);
                if (escaped)      { WCUP_BRANCH(1); WCUP_BRANCH(0); WCUP_BRANCH(0); escaped = 0; }
                else {
                    WCUP_BRANCH(0);
                    if (c == '\\')     { WCUP_BRANCH(1); WCUP_BRANCH(0); WCUP_BRANCH(0); escaped = 1; }
                    else if (c == '"') { WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_BRANCH(0); quote = 0; }
                    else                 { WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1); }
                }
            } else if (c == '\'') {          /* B4 */
                WCUP_BRANCH(0);
                WCUP_BRANCH(1); WCUP_BRANCH(0); WCUP_BRANCH(0);
                quote = '\'';
            } else if (c == '"') {           /* B5 */
                WCUP_BRANCH(0);
                WCUP_BRANCH(0); WCUP_BRANCH(1); WCUP_BRANCH(0);
                quote = '"';
            } else if (c == '\\') {          /* B6 */
                WCUP_BRANCH(0);
                WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
                i += 1;
            } else {
                WCUP_BRANCH(0);
                WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
            }
            i += 1;
        }
        result = quote;
    }
out:
    {
        char rbuf[2]; rbuf[0] = result ? result : ' '; rbuf[1] = '\0';
        WCUP_VALSTR(rbuf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("shell_quote_context_stt");
        return NULL;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s shell_quote_context_stt <tpl|pos>...\n", argv[0]); return 2; }
    for (int k = 2; k < argc; k++) {
        char *s = argv[k];
        char *bar = strrchr(s, '|');
        long pos = bar ? strtol(bar + 1, NULL, 10) : (long)strlen(s);
        if (bar) *bar = '\0';
        tsc_shell_quote_context_stt(s, pos);
    }
    return 0;
}
