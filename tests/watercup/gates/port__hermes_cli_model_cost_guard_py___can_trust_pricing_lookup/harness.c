/* gate: port__hermes_cli_model_cost_guard_py___can_trust_pricing_lookup — traced copy of _can_trust_pricing_lookup (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _can_trust_pricing_lookup(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _can_trust_pricing_lookup <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_can_trust_pricing_lookup");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _can_trust_pricing_lookup(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_can_trust_pricing_lookup");
    }
    return 0;
}
