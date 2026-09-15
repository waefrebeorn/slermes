/* gate: agent_account_usage_py___codex_backend_urls — traced copy of agent_account_usage_u_codex_backend_urls */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_account_usage_u_codex_backend_urls(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_account_usage_u_codex_backend_urls");
    int result = agent_account_usage_u_codex_backend_urls(input);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_account_usage_u_codex_backend_urls");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_account_usage_u_codex_backend_urls <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
