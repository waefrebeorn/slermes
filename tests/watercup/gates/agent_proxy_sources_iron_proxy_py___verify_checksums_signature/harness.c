/* gate: agent_proxy_sources_iron_proxy_py___verify_checksums_signature — traced copy of ipx_verify_checksums_signature */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ipx_verify_checksums_signature(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ipx_verify_checksums_signature");
    bool result = ipx_verify_checksums_signature(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ipx_verify_checksums_signature");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ipx_verify_checksums_signature <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
