/* gate: hermes_cli_model_data_policy_guard_py___is_meta_contributor — traced copy of _is_meta_contributor */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _is_meta_contributor(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_is_meta_contributor");
    int result = _is_meta_contributor(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_is_meta_contributor");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_meta_contributor <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
