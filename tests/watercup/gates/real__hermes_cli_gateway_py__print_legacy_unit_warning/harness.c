/* gate: real__hermes_cli_gateway_py__print_legacy_unit_warning — GAP sentinel for print_legacy_unit_warning */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "print_legacy_unit_warning";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s print_legacy_unit_warning <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("print_legacy_unit_warning");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("print_legacy_unit_warning");
    }
    return 0;
}
