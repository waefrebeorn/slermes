/* gate: hermes_cli_update_cmd_py___discard_lockfile_churn — traced copy of uc_discard_lockfile_churn */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int uc_discard_lockfile_churn(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("uc_discard_lockfile_churn");
    int result = uc_discard_lockfile_churn(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uc_discard_lockfile_churn");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_discard_lockfile_churn <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
