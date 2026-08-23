/* gate: clarify_tool_strip_recommended — traced copy of the REAL C port
 * src/tools/clarify_tool.c::clarify_strip_recommended
 * (PoP: tools/clarify_tool.py:strip_recommended).
 * LIVE twin: tools/clarify_tool.py::strip_recommended.
 *
 * Ladder (twin.py mirrors EXACTLY):
 *   B1: NULL/empty input ("\x00" sentinel) -> ""
 *   B2: len >= label length
 *   B3: suffix matches "(Recommended)" (ASCII case-insensitive)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static const char *LABEL = "(Recommended)";

static void str_strip_inplace(char *s) {
    char *a = s, *b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    if (a != s) memmove(s, a, b - a);
    s[b - a] = '\0';
}

static void run_one(const char *text) {
    WCUP_BEGIN("strip_recommended");
    char out[1024];
    if (!text) {                       /* B1 */
        WCUP_BRANCH(1);
        out[0] = '\0';
    } else {
        WCUP_BRANCH(0);
        snprintf(out, sizeof out, "%s", text);
        str_strip_inplace(out);
        size_t flen = strlen(out), labellen = strlen(LABEL);
        int cut = 0;
        if (flen >= labellen) {        /* B2 */
            WCUP_BRANCH(1);
            char *tail = out + flen - labellen;
            if (strcasecmp(tail, LABEL) == 0) {   /* B3 */
                WCUP_BRANCH(1);
                tail[0] = '\0';
                str_strip_inplace(out);
                cut = 1;
            } else WCUP_BRANCH(0);
        } else WCUP_BRANCH(0);
        (void)cut;
    }
    WCUP_VALSTR(out);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("strip_recommended");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s strip_recommended <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        run_one(argv[i]);
    return 0;
}
