/* gate: real__hermes_cli_sizefmt_py__format_bytes — GAP sentinel for format_bytes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "format_bytes";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s format_bytes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("format_bytes");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("format_bytes");
    }
    return 0;
}
