/* gate: tool_guardrails_as_bool — traced C copy of
 * src/agent/tool_guardrails_ports.c::tg_as_bool (PoP of LIVE
 * agent/tool_guardrails.py::_as_bool, string-rung subset).
 * WCUP records the same strcmp branch ladder the C port walks.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static char *lowerdup(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++)
        out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

static const char *tg_as_bool(const char *value, int default_value) {
    WCUP_BEGIN("tg_as_bool");
    const char *res;
    if (!value) {
        WCUP_BRANCH(1);
        res = default_value ? "1" : "0";
    } else {
        WCUP_BRANCH(0);
        char *l = lowerdup(value);
        int r = -1;
        if (strcmp(l, "true") == 0) { WCUP_BRANCH(1); r = 1; } else { WCUP_BRANCH(0);
        if (strcmp(l, "1") == 0) { WCUP_BRANCH(1); r = 1; } else { WCUP_BRANCH(0);
        if (strcmp(l, "yes") == 0) { WCUP_BRANCH(1); r = 1; } else { WCUP_BRANCH(0);
        if (strcmp(l, "on") == 0) { WCUP_BRANCH(1); r = 1; } else { WCUP_BRANCH(0);
        if (strcmp(l, "false") == 0) { WCUP_BRANCH(1); r = 0; } else { WCUP_BRANCH(0);
        if (strcmp(l, "0") == 0) { WCUP_BRANCH(1); r = 0; } else { WCUP_BRANCH(0);
        if (strcmp(l, "no") == 0) { WCUP_BRANCH(1); r = 0; } else { WCUP_BRANCH(0);
        if (strcmp(l, "off") == 0) { WCUP_BRANCH(1); r = 0; } else { WCUP_BRANCH(0);
        r = default_value;
        }}}}}}}}
        free(l);
        res = (r == 1) ? "1" : (r == 0) ? "0" : (default_value ? "1" : "0");
    }
    WCUP_VALSTR(res);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tg_as_bool");
    return res;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tg_as_bool <str>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        tg_as_bool(strcmp(argv[i], "null") == 0 ? NULL : argv[i], 1);
    return 0;
}
