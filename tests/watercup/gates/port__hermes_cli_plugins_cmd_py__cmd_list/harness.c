/* gate: port__hermes_cli_plugins_cmd_py__cmd_list — traced copy of cmd_list (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void cmd_list(int opt_json, int opt_plain, int opt_no_bundled_or_user,
              int opt_enabled_only);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cmd_list <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cmd_list");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cmd_list();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cmd_list");
    }
    return 0;
}
