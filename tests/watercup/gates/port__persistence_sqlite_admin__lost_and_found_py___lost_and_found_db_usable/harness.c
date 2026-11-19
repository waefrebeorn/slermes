/* gate: port__hermes_cli_session_lost_and_found_py___lost_and_found_db_usable — traced copy of laf_lost_and_found_db_usable (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int laf_lost_and_found_db_usable(const char *lf_path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s laf_lost_and_found_db_usable <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("laf_lost_and_found_db_usable");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = laf_lost_and_found_db_usable(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("laf_lost_and_found_db_usable");
    }
    return 0;
}
