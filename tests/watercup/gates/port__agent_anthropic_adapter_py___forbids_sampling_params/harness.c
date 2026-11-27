/* gate: port__agent_anthropic_adapter_py___forbids_sampling_params — traced copy of forbids_sampling_params (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool forbids_sampling_params(const char *model);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s forbids_sampling_params <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("forbids_sampling_params");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = forbids_sampling_params(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("forbids_sampling_params");
    }
    return 0;
}
