/* gate: clarify_tool_mark_recommended — traced copy of the REAL C port
 * src/tools/clarify_tool.c::clarify_mark_recommended
 * (PoP: tools/clarify_tool.py:mark_recommended).
 * LIVE twin: tools/clarify_tool.py::mark_recommended.
 *
 * Choices arrive as one argv element, '\x1f'-separated (argv-safe).
 *
 * Ladder (twin.py mirrors EXACTLY):
 *   B1: count < 2 -> passthrough
 *   B2: first choice already carries the label (ASCII case-insensitive
 *       suffix) -> passthrough
 *   else decorate first with " (Recommended)"
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

#define MAXC 8

static void run_one(const char *arg) {
    WCUP_BEGIN("mark_recommended");
    char choices[MAXC][256];
    int count = 0;
    {
        const char *p = arg ? arg : "";
        while (*p && count < MAXC) {
            size_t n = strcspn(p, "\x1f");
            if (n > 255) n = 255;
            memcpy(choices[count], p, n);
            choices[count][n] = '\0';
            count++;
            p += n;
            if (*p == '\x1f') p++;
        }
    }
    char out[2048]; out[0] = '\0';
    if (count < 2) {                       /* B1 */
        WCUP_BRANCH(1);
        for (int i = 0; i < count; i++)
            snprintf(out + strlen(out), sizeof out - strlen(out), "%s%s",
                     i ? "\x1f" : "", choices[i]);
    } else {
        WCUP_BRANCH(0);
        char first[256];
        snprintf(first, sizeof first, "%s", choices[0]);
        str_strip_inplace(first);
        size_t flen = strlen(first), labellen = strlen(LABEL);
        int already = flen >= labellen &&
            strcasecmp(first + flen - labellen, LABEL) == 0;
        if (already) {                     /* B2 */
            WCUP_BRANCH(1);
            for (int i = 0; i < count; i++)
                snprintf(out + strlen(out), sizeof out - strlen(out), "%s%s",
                         i ? "\x1f" : "", choices[i]);
        } else {
            WCUP_BRANCH(0);
            snprintf(out, sizeof out, "%s %s", first, LABEL);
            for (int i = 2; i < count; i++)
                snprintf(out + strlen(out), sizeof out - strlen(out), "%s%s",
                         "\x1f", choices[i]);
        }
    }
    WCUP_VALSTR(out);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mark_recommended");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mark_recommended <\\x1f-joined choices>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
