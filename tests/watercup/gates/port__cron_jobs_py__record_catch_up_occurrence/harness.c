/* gate: port__cron_jobs_py__record_catch_up_occurrence — traced copy of record_catch_up_occurrence (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void record_catch_up_occurrence(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s record_catch_up_occurrence <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("record_catch_up_occurrence");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    record_catch_up_occurrence(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("record_catch_up_occurrence");
    }
    return 0;
}
