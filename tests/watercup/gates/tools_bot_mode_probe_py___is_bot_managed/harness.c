/* gate: tools_bot_mode_probe_py___is_bot_managed — traced copy of bmp_is_bot_managed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int bmp_is_bot_managed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("bmp_is_bot_managed");
    int result = bmp_is_bot_managed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bmp_is_bot_managed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bmp_is_bot_managed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
