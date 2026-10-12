/* gate: port__cron_jobs_py___completed_oneshot_retention_days — traced copy of _completed_oneshot_retention_days (double_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern double _completed_oneshot_retention_days(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _completed_oneshot_retention_days <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_completed_oneshot_retention_days");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = _completed_oneshot_retention_days(strtod(argv[i], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_completed_oneshot_retention_days");
    }
    return 0;
}
