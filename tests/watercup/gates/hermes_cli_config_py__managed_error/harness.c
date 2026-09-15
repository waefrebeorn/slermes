/* gate: hermes_cli_config_py__managed_error — traced copy of cfg_managed_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cfg_managed_error(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cfg_managed_error");
    int result = cfg_managed_error(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cfg_managed_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cfg_managed_error <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
