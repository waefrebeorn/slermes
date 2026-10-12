/* gate: port__hermes_cli_memory_setup_py__cmd_setup_provider — traced copy of cli_hermes_cli_memory_setup_cmd_setup_provider (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void cli_hermes_cli_memory_setup_cmd_setup_provider(const char *provider_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_hermes_cli_memory_setup_cmd_setup_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_hermes_cli_memory_setup_cmd_setup_provider");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cli_hermes_cli_memory_setup_cmd_setup_provider(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_hermes_cli_memory_setup_cmd_setup_provider");
    }
    return 0;
}
