/* gate: port__gateway_systemd_notify_py___lag_tolerance — traced copy of sd_n_lag_tolerance (double_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern double sd_n_lag_tolerance(double interval_seconds, double configured);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sd_n_lag_tolerance <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sd_n_lag_tolerance");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = sd_n_lag_tolerance(strtod(argv[i], NULL), strtod(argv[2 + (i - 2 + 1) % 5], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sd_n_lag_tolerance");
    }
    return 0;
}
