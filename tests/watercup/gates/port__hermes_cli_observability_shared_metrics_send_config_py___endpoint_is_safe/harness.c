/* gate: {'name': 'port__hermes_cli_observability_shared_metrics_send_config_py___endpoint_is_safe', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_endpoint_is_safe', 'py_module': 'hermes_cli/observability/shared_metrics_send_config.py', 'py_fn': '_endpoint_is_safe', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/observability/shared_metrics_send_config_gaps3.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _endpoint_is_safe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_endpoint_is_safe";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _endpoint_is_safe <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_endpoint_is_safe");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_endpoint_is_safe");
    }
    return 0;
}
