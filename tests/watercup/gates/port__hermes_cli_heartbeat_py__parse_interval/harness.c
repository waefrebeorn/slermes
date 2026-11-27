/* gate: {'name': 'port__hermes_cli_heartbeat_py__parse_interval', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': 'None', 'py_module': 'hermes_cli/heartbeat.py', 'py_fn': 'parse_interval', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/hermes_cli/heartbeat.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for parse_interval */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "parse_interval";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_interval <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("parse_interval");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("parse_interval");
    }
    return 0;
}
