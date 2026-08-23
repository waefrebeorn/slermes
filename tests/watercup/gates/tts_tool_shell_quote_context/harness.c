/* gate: tts_tool_shell_quote_context — traced copy of
 * src/tools/tts_tools.c::tts_shell_quote_context (FIXED to match LIVE).
 * LIVE twin: tools/tts_tool.py::_shell_quote_context.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: null/negative position            -> 0
 *   B2: quote == '\''                     -> (V state machine)
 *   B3: quote == '"'                      -> escape/close handling
 *   B4/B5/B6: open ' / open " / bare backslash skip
 *
 * Input encoding: "<template>|<position>" (split on LAST '|').
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static char tts_shell_quote_context(const char *command, long position) {
    WCUP_BEGIN("shell_quote_context");
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
            } else if (c == '\\') {          /* B6: bare backslash skips next */
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
        WCUP_END("shell_quote_context");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s shell_quote_context <tpl|pos>...\n", argv[0]); return 2; }
    for (int k = 2; k < argc; k++) {
        char *s = argv[k];
        char *bar = strrchr(s, '|');
        long pos = bar ? strtol(bar + 1, NULL, 10) : (long)strlen(s);
        if (bar) *bar = '\0';
        tts_shell_quote_context(s, pos);
    }
    return 0;
}
