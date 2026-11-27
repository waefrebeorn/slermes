/* gate: {'name': 'hermes_cli_gateway_py___reaper_candidate_is_supervisor_owned', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_reaper_candidate_is_supervisor_owned', 'py_module': 'hermes_cli/gateway.py', 'py_fn': '_reaper_candidate_is_supervisor_owned', 'inputs': [0, 1, 42, 1024, -7], 'extra_src': ['lib/libjson/json.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src']} — GAP sentinel for _reaper_candidate_is_supervisor_owned */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_reaper_candidate_is_supervisor_owned";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _reaper_candidate_is_supervisor_owned <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_reaper_candidate_is_supervisor_owned");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_reaper_candidate_is_supervisor_owned");
    }
    return 0;
}
