/* gate: port__hermes_cli_observability_relay_shared_metrics_py___reset_for_tests — traced copy of rsm_reset_for_tests (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int rsm_reset_for_tests(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rsm_reset_for_tests <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("rsm_reset_for_tests");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = rsm_reset_for_tests();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("rsm_reset_for_tests");
    }
    return 0;
}
