/* gate: image_routing_coerce_capability_bool — traced C copy of
 * src/agent/image_routing_ports.c::imgr_coerce_capability_bool (PoP of LIVE
 * agent/image_routing.py::_coerce_capability_bool, string-rung subset).
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

static const char *imgr_coerce_capability_bool(const char *value) {
    WCUP_BEGIN("coerce_capability_bool");
    const char *res;
    if (!value) {
        WCUP_BRANCH(1);
        res = "-1"; /* None */
    } else {
        WCUP_BRANCH(0);
        char *l = lowerdup(value);
        int r = -1;
        if (strcmp(l, "true") == 0) { WCUP_BRANCH(1); r = 1; } else { WCUP_BRANCH(0);
        if (strcmp(l, "1") == 0) { WCUP_BRANCH(1); r = 1; } else { WCUP_BRANCH(0);
        if (strcmp(l, "yes") == 0) { WCUP_BRANCH(1); r = 1; } else { WCUP_BRANCH(0);
        if (strcmp(l, "false") == 0) { WCUP_BRANCH(1); r = 0; } else { WCUP_BRANCH(0);
        if (strcmp(l, "0") == 0) { WCUP_BRANCH(1); r = 0; } else { WCUP_BRANCH(0);
        if (strcmp(l, "no") == 0) { WCUP_BRANCH(1); r = 0; } else { WCUP_BRANCH(0);
        r = -1;
        }}}}}}
        free(l);
        static char buf[4];
        snprintf(buf, sizeof buf, "%d", r);
        res = buf;
    }
    WCUP_VALSTR(res);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("coerce_capability_bool");
    return res;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s imgr_coerce_capability_bool <str>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        imgr_coerce_capability_bool(strcmp(argv[i], "null") == 0 ? NULL : argv[i]);
    return 0;
}
