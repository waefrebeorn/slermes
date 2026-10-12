/* gate: port__hermes_cli_setup_py___model_section_has_credentials — traced copy of stp_model_section_has_credentials (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool stp_model_section_has_credentials(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stp_model_section_has_credentials <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("stp_model_section_has_credentials");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = stp_model_section_has_credentials();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("stp_model_section_has_credentials");
    }
    return 0;
}
