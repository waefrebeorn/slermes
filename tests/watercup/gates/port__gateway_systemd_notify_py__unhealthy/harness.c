/* gate: port__gateway_systemd_notify_py__unhealthy — traced copy of sd_n_unhealthy_state (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool sd_n_unhealthy_state(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sd_n_unhealthy_state <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sd_n_unhealthy_state");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = sd_n_unhealthy_state();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sd_n_unhealthy_state");
    }
    return 0;
}
