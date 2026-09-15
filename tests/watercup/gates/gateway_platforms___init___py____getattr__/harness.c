/* gate: gateway_platforms___init___py____getattr__ — traced copy of gw_platforms_getattr */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_platforms_getattr(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_platforms_getattr");
    int result = gw_platforms_getattr(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_platforms_getattr");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_platforms_getattr <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
