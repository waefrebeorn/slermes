/* gate: agent_ssl_verify_py___coerce_insecure — traced copy of agent_ssl_verify_coerce_insecure */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool agent_ssl_verify_coerce_insecure(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_ssl_verify_coerce_insecure");
    bool result = agent_ssl_verify_coerce_insecure(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_ssl_verify_coerce_insecure");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_ssl_verify_coerce_insecure <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
