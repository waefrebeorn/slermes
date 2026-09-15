/* gate: gateway_platforms_base_py___stop_typing_refresh — traced copy of gw_base__stop_typing_refresh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_base__stop_typing_refresh(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_base__stop_typing_refresh");
    int result = gw_base__stop_typing_refresh(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_base__stop_typing_refresh");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_base__stop_typing_refresh <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
