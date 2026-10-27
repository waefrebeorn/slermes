/* gate: port__hermes_cli_setup_py__prompt_yes_no — traced copy of stp_prompt_yes_no (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool stp_prompt_yes_no(const char *question, bool default_value);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stp_prompt_yes_no <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("stp_prompt_yes_no");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = stp_prompt_yes_no(argv[i], (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("stp_prompt_yes_no");
    }
    return 0;
}
