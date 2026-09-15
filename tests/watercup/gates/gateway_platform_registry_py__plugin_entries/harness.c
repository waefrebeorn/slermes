/* gate: gateway_platform_registry_py__plugin_entries — traced copy of gateway_platform_registry_plugin_entries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_platform_registry_plugin_entries(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_platform_registry_plugin_entries");
    int result = gateway_platform_registry_plugin_entries(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_platform_registry_plugin_entries");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_platform_registry_plugin_entries <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
