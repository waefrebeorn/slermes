/* gate: gateway_config_py___is_platform_connected — traced copy of gcf_is_platform_connected */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gcf_is_platform_connected(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gcf_is_platform_connected");
    int result = gcf_is_platform_connected(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gcf_is_platform_connected");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gcf_is_platform_connected <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
