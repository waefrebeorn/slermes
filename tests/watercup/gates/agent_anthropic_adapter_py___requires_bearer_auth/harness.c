/* gate: agent_anthropic_adapter_py___requires_bearer_auth — traced copy of antr_requires_bearer_auth */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int antr_requires_bearer_auth(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("antr_requires_bearer_auth");
    int result = antr_requires_bearer_auth(input);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("antr_requires_bearer_auth");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s antr_requires_bearer_auth <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
