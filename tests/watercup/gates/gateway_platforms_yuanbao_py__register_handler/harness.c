/* gate: gateway_platforms_yuanbao_py__register_handler — traced copy of yb_register_handler */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_register_handler(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_register_handler");
    int result = yb_register_handler(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_register_handler");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_register_handler <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
