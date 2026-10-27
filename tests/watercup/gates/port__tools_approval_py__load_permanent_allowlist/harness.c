/* gate: port__tools_approval_py__load_permanent_allowlist — traced copy of apr_load_permanent_allowlist (long_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern long apr_load_permanent_allowlist(const char *config_yaml);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr_load_permanent_allowlist <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("apr_load_permanent_allowlist");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = apr_load_permanent_allowlist(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("apr_load_permanent_allowlist");
    }
    return 0;
}
