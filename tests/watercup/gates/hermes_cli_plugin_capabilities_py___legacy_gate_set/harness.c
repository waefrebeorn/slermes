/* gate: hermes_cli_plugin_capabilities_py___legacy_gate_set — traced copy of pc_legacy_gate_set */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pc_legacy_gate_set(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("pc_legacy_gate_set");
    int result = pc_legacy_gate_set(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pc_legacy_gate_set");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pc_legacy_gate_set <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
