/* gate: port__agent_anthropic_adapter_py___is_claude_model — traced copy of antr_is_claude_model (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool antr_is_claude_model(const char *model);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s antr_is_claude_model <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("antr_is_claude_model");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = antr_is_claude_model(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("antr_is_claude_model");
    }
    return 0;
}
