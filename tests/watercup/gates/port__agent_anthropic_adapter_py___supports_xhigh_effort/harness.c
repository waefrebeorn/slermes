/* gate: port__agent_anthropic_adapter_py___supports_xhigh_effort — traced copy of supports_xhigh_effort (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool supports_xhigh_effort(const char *model);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s supports_xhigh_effort <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("supports_xhigh_effort");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = supports_xhigh_effort(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("supports_xhigh_effort");
    }
    return 0;
}
