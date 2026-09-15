/* gate: tools_skill_usage_py__archive_skill — traced copy of su_archive_skill */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int su_archive_skill(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("su_archive_skill");
    int result = su_archive_skill(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("su_archive_skill");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s su_archive_skill <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
