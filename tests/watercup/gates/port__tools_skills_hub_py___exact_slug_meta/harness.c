/* gate: port__tools_skills_hub_py___exact_slug_meta — traced copy of sh_exact_slug_meta (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * sh_exact_slug_meta(const char *query, const char *results_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sh_exact_slug_meta <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sh_exact_slug_meta");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = sh_exact_slug_meta(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sh_exact_slug_meta");
    }
    return 0;
}
