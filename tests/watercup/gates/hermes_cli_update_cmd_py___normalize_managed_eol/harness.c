/* gate: hermes_cli_update_cmd_py___normalize_managed_eol — traced copy of uc_normalize_managed_eol */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int uc_normalize_managed_eol(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("uc_normalize_managed_eol");
    int result = uc_normalize_managed_eol(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uc_normalize_managed_eol");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_normalize_managed_eol <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
