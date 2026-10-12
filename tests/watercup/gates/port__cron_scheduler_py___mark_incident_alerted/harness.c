/* gate: port__cron_scheduler_py___mark_incident_alerted — traced copy of _mark_incident_alerted (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _mark_incident_alerted(const char *job_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _mark_incident_alerted <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_mark_incident_alerted");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _mark_incident_alerted(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_mark_incident_alerted");
    }
    return 0;
}
