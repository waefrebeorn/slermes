/* gate: agent_monitoring_otlp_exporter_py__is_enabled — traced copy of otlp_exporter_is_enabled */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool otlp_exporter_is_enabled(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("otlp_exporter_is_enabled");
    bool result = otlp_exporter_is_enabled(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("otlp_exporter_is_enabled");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s otlp_exporter_is_enabled <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
