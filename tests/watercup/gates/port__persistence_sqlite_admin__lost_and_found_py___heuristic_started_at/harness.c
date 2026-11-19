/* gate: port__persistence_sqlite_admin__lost_and_found_py___heuristic_started_at — traced copy of laf_heuristic_started_at (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern double laf_heuristic_started_at(const char *cells_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s laf_heuristic_started_at <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("laf_heuristic_started_at");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = laf_heuristic_started_at(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("laf_heuristic_started_at");
    }
    return 0;
}
