/* gate: port__cron_scheduler_py___detect_gateway_code_skew — traced copy of _detect_gateway_code_skew (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int _detect_gateway_code_skew(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _detect_gateway_code_skew <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_detect_gateway_code_skew");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _detect_gateway_code_skew();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_detect_gateway_code_skew");
    }
    return 0;
}
