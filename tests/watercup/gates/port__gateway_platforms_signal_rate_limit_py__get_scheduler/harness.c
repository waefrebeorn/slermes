/* gate: port__gateway_platforms_signal_rate_limit_py__get_scheduler — traced copy of srl_get_scheduler (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int srl_get_scheduler(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s srl_get_scheduler <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("srl_get_scheduler");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = srl_get_scheduler();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("srl_get_scheduler");
    }
    return 0;
}
