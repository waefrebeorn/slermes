/* gate: port__agent_account_usage_py___resolve_codex_usage_credentials — traced copy of agent_account_usage_u_resolve_codex_usage_credentials (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int agent_account_usage_u_resolve_codex_usage_credentials(const char *arg);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_account_usage_u_resolve_codex_usage_credentials <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("agent_account_usage_u_resolve_codex_usage_credentials");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = agent_account_usage_u_resolve_codex_usage_credentials(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("agent_account_usage_u_resolve_codex_usage_credentials");
    }
    return 0;
}
