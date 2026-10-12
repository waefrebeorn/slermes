/* gate: port__agent_anthropic_adapter_py___get_claude_code_version — traced copy of antr_get_claude_code_version (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool antr_get_claude_code_version(const char *key);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s antr_get_claude_code_version <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("antr_get_claude_code_version");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = antr_get_claude_code_version(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("antr_get_claude_code_version");
    }
    return 0;
}
