/* gate: port__cron_scheduler_py___upsert_incident_for_failure — traced copy of _upsert_incident_for_failure (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _upsert_incident_for_failure(const char *job_id, const char *error);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _upsert_incident_for_failure <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_upsert_incident_for_failure");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _upsert_incident_for_failure(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_upsert_incident_for_failure");
    }
    return 0;
}
