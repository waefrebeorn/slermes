/* gate: real__hermes_cli_personality_py__active_personality_name — GAP sentinel for active_personality_name */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "active_personality_name";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s active_personality_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("active_personality_name");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("active_personality_name");
    }
    return 0;
}
