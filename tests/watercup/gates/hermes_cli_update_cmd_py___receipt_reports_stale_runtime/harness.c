/* gate: hermes_cli_update_cmd_py___receipt_reports_stale_runtime — traced copy of _receipt_reports_stale_runtime */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _receipt_reports_stale_runtime(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_receipt_reports_stale_runtime");
    int result = _receipt_reports_stale_runtime(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_receipt_reports_stale_runtime");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _receipt_reports_stale_runtime <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
