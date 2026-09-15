/* gate: tools_bot_mode_probe_py___soul_has_protocol — traced copy of bmp_soul_has_protocol */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int bmp_soul_has_protocol(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("bmp_soul_has_protocol");
    int result = bmp_soul_has_protocol(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bmp_soul_has_protocol");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bmp_soul_has_protocol <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
