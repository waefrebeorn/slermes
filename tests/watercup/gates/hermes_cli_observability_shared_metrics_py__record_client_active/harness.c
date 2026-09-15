/* gate: hermes_cli_observability_shared_metrics_py__record_client_active — traced copy of metrics_record_client_active */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int metrics_record_client_active(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("metrics_record_client_active");
    int result = metrics_record_client_active(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("metrics_record_client_active");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s metrics_record_client_active <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
