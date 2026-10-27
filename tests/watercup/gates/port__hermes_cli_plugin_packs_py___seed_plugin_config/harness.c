/* gate: port__hermes_cli_plugin_packs_py___seed_plugin_config — traced copy of _seed_plugin_config (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void _seed_plugin_config(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _seed_plugin_config <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_seed_plugin_config");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _seed_plugin_config();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_seed_plugin_config");
    }
    return 0;
}
