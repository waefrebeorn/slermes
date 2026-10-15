/* gate: port__hermes_cli_model_cost_guard_py___format_money — traced copy of model_cost_guard_format_money (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void model_cost_guard_format_money(const double *value, char *out, size_t out_sz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s model_cost_guard_format_money <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("model_cost_guard_format_money");
    double _o0 = 0;
    char _s1[256]; _s1[0] = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    model_cost_guard_format_money(&_o0, _s1, sizeof(_s1));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[64]; snprintf(_b, sizeof(_b), "%.17g", (double)_o0); WCUP_VALSTR(_b); }
    WCUP_VALSTR(_s1);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("model_cost_guard_format_money");
    }
    return 0;
}
