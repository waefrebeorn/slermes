/* gate: agent_monitoring_otlp_exporter_py___require_sdk — traced copy of otlp_exporter_require_sdk */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool otlp_exporter_require_sdk(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("otlp_exporter_require_sdk");
    bool result = otlp_exporter_require_sdk(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("otlp_exporter_require_sdk");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s otlp_exporter_require_sdk <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
