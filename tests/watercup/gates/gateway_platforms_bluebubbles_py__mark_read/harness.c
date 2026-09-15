/* gate: gateway_platforms_bluebubbles_py__mark_read — traced copy of bb_mark_read */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int bb_mark_read(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("bb_mark_read");
    int result = bb_mark_read(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bb_mark_read");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bb_mark_read <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
