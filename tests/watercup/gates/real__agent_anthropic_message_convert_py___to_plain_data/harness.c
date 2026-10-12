/* gate: real__agent_anthropic_message_convert_py___to_plain_data — GAP sentinel for _to_plain_data */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_to_plain_data";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _to_plain_data <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_to_plain_data");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_to_plain_data");
    }
    return 0;
}
