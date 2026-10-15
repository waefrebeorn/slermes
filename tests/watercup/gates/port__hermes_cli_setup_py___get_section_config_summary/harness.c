/* gate: port__hermes_cli_setup_py___get_section_config_summary — traced copy of _get_section_config_summary (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _get_section_config_summary(const char *section);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _get_section_config_summary <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_get_section_config_summary");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _get_section_config_summary(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_get_section_config_summary");
    }
    return 0;
}
