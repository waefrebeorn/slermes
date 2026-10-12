/* gate: port__agent_display_py___result_succeeded — traced copy of disp_result_succeeded (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool disp_result_succeeded(const char *result);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s disp_result_succeeded <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("disp_result_succeeded");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = disp_result_succeeded(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("disp_result_succeeded");
    }
    return 0;
}
