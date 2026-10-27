/* gate: port__agent_credential_pool_py___exhausted_ttl — traced copy of cpl_exhausted_ttl (long_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern long cpl_exhausted_ttl(long error_code);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cpl_exhausted_ttl <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cpl_exhausted_ttl");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = cpl_exhausted_ttl((long)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cpl_exhausted_ttl");
    }
    return 0;
}
