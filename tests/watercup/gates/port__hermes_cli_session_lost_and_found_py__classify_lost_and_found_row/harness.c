/* gate: port__hermes_cli_session_lost_and_found_py__classify_lost_and_found_row — traced copy of laf_classify_row (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * laf_classify_row(int nfield, const char *cells_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s laf_classify_row <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("laf_classify_row");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = laf_classify_row(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("laf_classify_row");
    }
    return 0;
}
