/* gate: port__hermes_cli_update_cmd_py___repair_node_deps_on_current_checkout — traced copy of _repair_node_deps_on_current_checkout (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int _repair_node_deps_on_current_checkout(const char *print_completion, int assume_yes, int gateway_mode, const char *pre_update_snapshot_id, const char *completion_message, int had_desktop_app_before_update);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _repair_node_deps_on_current_checkout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_repair_node_deps_on_current_checkout");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _repair_node_deps_on_current_checkout(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5], (int)strtol(argv[2 + (i - 2 + 5) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_repair_node_deps_on_current_checkout");
    }
    return 0;
}
