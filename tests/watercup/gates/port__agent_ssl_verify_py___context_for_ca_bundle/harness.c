/* gate: port__agent_ssl_verify_py___context_for_ca_bundle — traced copy of _context_for_ca_bundle (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _context_for_ca_bundle(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _context_for_ca_bundle <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_context_for_ca_bundle");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _context_for_ca_bundle(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_context_for_ca_bundle");
    }
    return 0;
}
