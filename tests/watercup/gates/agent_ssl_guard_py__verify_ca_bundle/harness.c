/* gate: agent_ssl_guard_py__verify_ca_bundle — traced copy of sslg_verify_ca_bundle */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sslg_verify_ca_bundle(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sslg_verify_ca_bundle");
    int result = sslg_verify_ca_bundle(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sslg_verify_ca_bundle");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sslg_verify_ca_bundle <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
