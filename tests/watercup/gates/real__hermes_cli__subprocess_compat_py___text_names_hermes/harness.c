/* gate: real__hermes_cli__subprocess_compat_py___text_names_hermes — GAP sentinel for _text_names_hermes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_text_names_hermes";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _text_names_hermes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_text_names_hermes");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_text_names_hermes");
    }
    return 0;
}
