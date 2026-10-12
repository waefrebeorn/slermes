/* gate: port__hermes_cli_update_cmd_py___restore_active_tool_dependencies — traced copy of _restore_active_tool_dependencies (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _restore_active_tool_dependencies(const char *dependencies, const char *install_cmd_prefix, const char *env);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _restore_active_tool_dependencies <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_restore_active_tool_dependencies");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _restore_active_tool_dependencies(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_restore_active_tool_dependencies");
    }
    return 0;
}
