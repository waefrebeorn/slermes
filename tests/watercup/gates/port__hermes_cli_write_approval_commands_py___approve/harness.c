/* gate: port__hermes_cli_write_approval_commands_py___approve — traced copy of cli_hermes_cli_write_approval_commands__approve (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int cli_hermes_cli_write_approval_commands__approve(const char *subsystem, const char *target,
    char *output, size_t output_size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_hermes_cli_write_approval_commands__approve <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_hermes_cli_write_approval_commands__approve");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_hermes_cli_write_approval_commands__approve(argv[i], argv[2 + (i - 2 + 1) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_hermes_cli_write_approval_commands__approve");
    }
    return 0;
}
