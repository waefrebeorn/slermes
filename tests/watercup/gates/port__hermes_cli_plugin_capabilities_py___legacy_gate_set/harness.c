/* gate: port__hermes_cli_plugin_capabilities_py___legacy_gate_set — traced copy of pc_legacy_gate_set (int_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int pc_legacy_gate_set(const char *entry_json, const char *capability);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pc_legacy_gate_set <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("pc_legacy_gate_set");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = pc_legacy_gate_set(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pc_legacy_gate_set");
    }
    return 0;
}
