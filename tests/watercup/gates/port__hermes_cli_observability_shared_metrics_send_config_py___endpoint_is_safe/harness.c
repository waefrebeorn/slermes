/* gate: port__hermes_cli_observability_shared_metrics_send_config_py___endpoint_is_safe — traced copy of _endpoint_is_safe (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _endpoint_is_safe(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _endpoint_is_safe <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_endpoint_is_safe");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _endpoint_is_safe(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_endpoint_is_safe");
    }
    return 0;
}
