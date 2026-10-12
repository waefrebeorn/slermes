/* gate: port__hermes_cli_observability_shared_metrics_py___add_consent_tables — traced copy of _add_consent_tables (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _add_consent_tables(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _add_consent_tables <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_add_consent_tables");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _add_consent_tables(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_add_consent_tables");
    }
    return 0;
}
