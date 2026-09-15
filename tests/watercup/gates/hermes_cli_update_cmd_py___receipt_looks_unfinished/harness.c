/* gate: hermes_cli_update_cmd_py___receipt_looks_unfinished — traced copy of _receipt_looks_unfinished */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _receipt_looks_unfinished(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_receipt_looks_unfinished");
    int result = _receipt_looks_unfinished(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_receipt_looks_unfinished");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _receipt_looks_unfinished <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
