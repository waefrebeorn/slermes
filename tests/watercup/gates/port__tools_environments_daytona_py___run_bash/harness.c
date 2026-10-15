/* gate: port__tools_environments_daytona_py___run_bash — traced copy of cli_tools_environments_daytona__run_bash (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_tools_environments_daytona__run_bash(const char *sandbox_id, const char *cmd_string,
    int login, int timeout, const char *stdin_data,
    char *output_out, size_t output_size, int *exit_code_out);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_environments_daytona__run_bash <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_environments_daytona__run_bash");
    char _s5[256]; _s5[0] = 0;
    int _o7 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_tools_environments_daytona__run_bash(argv[i], argv[2 + (i - 2 + 1) % 5], (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), argv[2 + (i - 2 + 4) % 5], _s5, sizeof(_s5), &_o7);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_s5);
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o7); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_environments_daytona__run_bash");
    }
    return 0;
}
