/* gate: port__gateway_scale_to_zero_py__parse_idle_timeout_seconds — traced copy of scale_to_zero_parse_idle_timeout_seconds (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern double scale_to_zero_parse_idle_timeout_seconds(double cfg_value, int has_cfg, int default_minutes);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s scale_to_zero_parse_idle_timeout_seconds <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("scale_to_zero_parse_idle_timeout_seconds");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = scale_to_zero_parse_idle_timeout_seconds(strtod(argv[i], NULL), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("scale_to_zero_parse_idle_timeout_seconds");
    }
    return 0;
}
