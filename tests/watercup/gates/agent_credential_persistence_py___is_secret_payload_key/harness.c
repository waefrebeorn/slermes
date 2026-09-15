/* gate: agent_credential_persistence_py___is_secret_payload_key — traced copy of is_secret_payload_key */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_secret_payload_key(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_secret_payload_key");
    bool result = is_secret_payload_key(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_secret_payload_key");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_secret_payload_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
