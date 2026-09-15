/* gate: tools_skill_usage_py__bump_patch — traced copy of su_bump_patch */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int su_bump_patch(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("su_bump_patch");
    int result = su_bump_patch(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("su_bump_patch");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s su_bump_patch <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
