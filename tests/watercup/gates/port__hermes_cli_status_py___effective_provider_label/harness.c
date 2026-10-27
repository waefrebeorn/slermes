/* gate: port__hermes_cli_status_py___effective_provider_label — traced copy of sta_effective_provider_label (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void sta_effective_provider_label(char out[256]);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sta_effective_provider_label <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sta_effective_provider_label");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    sta_effective_provider_label();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sta_effective_provider_label");
    }
    return 0;
}
