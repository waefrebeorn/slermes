/* gate: {'name': 'port__cron_scheduler_py___inchannel_seed_allowed', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_inchannel_seed_allowed', 'py_module': 'cron/scheduler.py', 'py_fn': '_inchannel_seed_allowed', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/cron/scheduler_gaps.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for _inchannel_seed_allowed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_inchannel_seed_allowed";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _inchannel_seed_allowed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_inchannel_seed_allowed");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_inchannel_seed_allowed");
    }
    return 0;
}
