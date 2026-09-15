/* gate: gateway_platforms_weixin_py___persist — traced copy of wx2_persist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wx2_persist(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("wx2_persist");
    int result = wx2_persist(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wx2_persist");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wx2_persist <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
