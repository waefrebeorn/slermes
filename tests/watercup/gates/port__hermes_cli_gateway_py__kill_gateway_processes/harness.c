/* gate: port__hermes_cli_gateway_py__kill_gateway_processes — traced copy of kill_gateway_processes (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int kill_gateway_processes(bool force, const int *exclude_pids,
                           size_t n_exclude, bool all_profiles);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kill_gateway_processes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("kill_gateway_processes");
    int _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = kill_gateway_processes((int)strtol(argv[i], NULL, 10), &_o1, (long)strtol(argv[2 + (i - 2 + 2) % 2], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 2], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o1); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("kill_gateway_processes");
    }
    return 0;
}
