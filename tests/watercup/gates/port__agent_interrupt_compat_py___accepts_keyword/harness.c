/* gate: port__agent_interrupt_compat_py___accepts_keyword — traced copy of agent_interrupt_compat_accepts_keyword (bool_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool agent_interrupt_compat_accepts_keyword(const char *callable_name, const char *param_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_interrupt_compat_accepts_keyword <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("agent_interrupt_compat_accepts_keyword");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = agent_interrupt_compat_accepts_keyword(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("agent_interrupt_compat_accepts_keyword");
    }
    return 0;
}
