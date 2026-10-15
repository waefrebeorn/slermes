/* gate: port__agent_anthropic_adapter_py___get_anthropic_sdk — traced copy of cli_agent_anthropic_adapter__get_anthropic_sdk (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern char* cli_agent_anthropic_adapter__get_anthropic_sdk(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_anthropic_adapter__get_anthropic_sdk <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_agent_anthropic_adapter__get_anthropic_sdk");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char* result = cli_agent_anthropic_adapter__get_anthropic_sdk();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_agent_anthropic_adapter__get_anthropic_sdk");
    }
    return 0;
}
