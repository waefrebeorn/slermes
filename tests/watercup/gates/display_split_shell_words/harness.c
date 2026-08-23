/* gate: display_split_shell_words — traced copy of
 * src/hermes_cli/display.c::_cli_split_shell_words
 * (PoP: agent/display.py:_split_shell_words).
 * Input: one raw segment string. Output V event: words joined by \x1f.
 * DOCUMENTED DIVERGENCE: LIVE ends a quote only when prev char != backslash;
 * C ends it unconditionally — escaped-quote inputs excluded. LIVE isspace()
 * covers \x0b/\x0c; C only space/tab/newline/CR — those excluded too.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void run_one(const char *segment) {
    char joined[8192]; size_t joff = 0; joined[0] = '\0';
    WCUP_BEGIN("split_shell_words");
    const char *p = segment;
    char buf[4096]; size_t blen = 0;
    int in_word = 0;
    char quote = 0;
    while (*p) {
        WCUP_LOOP_TICK();
        char c = *p;
        if (quote) {
            WCUP_BRANCH(1);
            if (blen + 1 < sizeof buf) buf[blen++] = c;
            if (c == quote) quote = 0;
            p++;
            in_word = 1;
            continue;
        } else WCUP_BRANCH(0);
        if (c == '\'' || c == '"') {
            WCUP_BRANCH(1);
            quote = c;
            if (!in_word) { WCUP_BRANCH(1); in_word = 1; blen = 0; }
            else WCUP_BRANCH(0);
            if (blen + 1 < sizeof buf) buf[blen++] = c;
            p++;
            continue;
        } else WCUP_BRANCH(0);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            WCUP_BRANCH(1);
            if (in_word) {
                WCUP_BRANCH(1);
                buf[blen] = '\0';
                if (joff) { joined[joff++] = '\x1f'; }
                strcpy(joined + joff, buf); joff += strlen(buf);
                in_word = 0; blen = 0;
            } else WCUP_BRANCH(0);
            p++;
            continue;
        } else WCUP_BRANCH(0);
        if (!in_word) { WCUP_BRANCH(1); in_word = 1; blen = 0; }
        else WCUP_BRANCH(0);
        if (blen + 1 < sizeof buf) buf[blen++] = c;
        p++;
    }
    if (in_word) {
        WCUP_BRANCH(1);
        buf[blen] = '\0';
        if (joff) joined[joff++] = '\x1f';
        strcpy(joined + joff, buf); joff += strlen(buf);
    } else WCUP_BRANCH(0);
    WCUP_VALSTR(joined);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("split_shell_words");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s split_shell_words <segment>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
