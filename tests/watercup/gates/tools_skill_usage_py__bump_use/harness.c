/* gate: tools_skill_usage_py__bump_use — traced copy of su_bump_use */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int su_bump_use(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("su_bump_use");
    int result = su_bump_use(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("su_bump_use");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s su_bump_use <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
