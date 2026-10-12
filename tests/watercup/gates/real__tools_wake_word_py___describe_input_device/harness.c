/* gate: real__tools_wake_word_py___describe_input_device — GAP sentinel for _describe_input_device */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_describe_input_device";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _describe_input_device <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_describe_input_device");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_describe_input_device");
    }
    return 0;
}
