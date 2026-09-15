/* gate: gateway_platforms_yuanbao_py__acquire_file — traced copy of yb_acquire_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_acquire_file(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_acquire_file");
    int result = yb_acquire_file(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_acquire_file");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_acquire_file <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
