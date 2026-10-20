/* gate: port__hermes_cli_gateway_py__launchd_gateway_labels_for_install — traced copy of launchd_gateway_labels_for_install (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char ** launchd_gateway_labels_for_install(size_t *out_n);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s launchd_gateway_labels_for_install <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("launchd_gateway_labels_for_install");
    size_t _o0 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char ** result = launchd_gateway_labels_for_install(&_o0);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%zu", (size_t)_o0); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("launchd_gateway_labels_for_install");
    }
    return 0;
}
