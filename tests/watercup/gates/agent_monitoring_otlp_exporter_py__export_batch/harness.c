/* gate: agent_monitoring_otlp_exporter_py__export_batch — traced copy of otlp_exporter_export_batch */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int otlp_exporter_export_batch(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("otlp_exporter_export_batch");
    int result = otlp_exporter_export_batch(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("otlp_exporter_export_batch");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s otlp_exporter_export_batch <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
