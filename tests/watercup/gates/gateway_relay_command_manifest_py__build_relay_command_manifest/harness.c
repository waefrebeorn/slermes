/* gate: gateway_relay_command_manifest_py__build_relay_command_manifest — traced copy of gateway_relay_command_manifest_build_relay_command_manifest */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_relay_command_manifest_build_relay_command_manifest(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_relay_command_manifest_build_relay_command_manifest");
    int result = gateway_relay_command_manifest_build_relay_command_manifest(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_relay_command_manifest_build_relay_command_manifest");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_relay_command_manifest_build_relay_command_manifest <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
