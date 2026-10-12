/* gate: real__hermes_cli_doctor_py___unreadable_reason — GAP sentinel for _unreadable_reason */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_unreadable_reason";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _unreadable_reason <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_unreadable_reason");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_unreadable_reason");
    }
    return 0;
}
