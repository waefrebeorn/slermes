/* gate: port__tools_computer_use_backend_py__center — traced copy of cli_tools_computer_use_backend_center (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cli_tools_computer_use_backend_center(int *x, int *y);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_computer_use_backend_center <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_tools_computer_use_backend_center");
    int _o0 = 0;
    int _o1 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cli_tools_computer_use_backend_center(&_o0, &_o1);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o0); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o1); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_tools_computer_use_backend_center");
    }
    return 0;
}
