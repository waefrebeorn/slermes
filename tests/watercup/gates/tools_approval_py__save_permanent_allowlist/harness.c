/* gate: tools_approval_py__save_permanent_allowlist — traced copy of apr_save_permanent_allowlist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int apr_save_permanent_allowlist(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("apr_save_permanent_allowlist");
    int result = apr_save_permanent_allowlist(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apr_save_permanent_allowlist");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr_save_permanent_allowlist <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
