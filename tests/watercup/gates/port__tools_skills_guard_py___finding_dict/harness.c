/* gate: port__tools_skills_guard_py___finding_dict — traced copy of getattr (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void getattr(char *buf, size_t *off, size_t cap, const char *s);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s getattr <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("getattr");
    char _s0[256]; _s0[0] = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    getattr(_s0, sizeof(_s0), (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), argv[2 + (i - 2 + 3) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_s0);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("getattr");
    }
    return 0;
}
