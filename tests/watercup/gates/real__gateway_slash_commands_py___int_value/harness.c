/* gate: real__gateway_slash_commands_py___int_value — GAP sentinel for _int_value */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_int_value";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _int_value <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_int_value");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_int_value");
    }
    return 0;
}
