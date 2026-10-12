/* gate: port__hermes_cli_setup_py___record_send_consent_change — traced copy of _record_send_consent_change (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _record_send_consent_change(const char *consent);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _record_send_consent_change <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_record_send_consent_change");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _record_send_consent_change(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_record_send_consent_change");
    }
    return 0;
}
