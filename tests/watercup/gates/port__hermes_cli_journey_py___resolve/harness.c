/* gate: port__hermes_cli_journey_py___resolve — traced copy of jny_resolve (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * jny_resolve(const char *ink_hex, const char *bg_hex, double alpha);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s jny_resolve <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("jny_resolve");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = jny_resolve(argv[i], argv[2 + (i - 2 + 1) % 5], strtod(argv[2 + (i - 2 + 2) % 5], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("jny_resolve");
    }
    return 0;
}
