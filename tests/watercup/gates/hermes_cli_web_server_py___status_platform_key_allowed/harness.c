/* gate: {'name': 'hermes_cli_web_server_py___status_platform_key_allowed', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_status_platform_key_allowed', 'py_module': 'hermes_cli/web_server.py', 'py_fn': '_status_platform_key_allowed', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src']} — GAP sentinel for _status_platform_key_allowed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_status_platform_key_allowed";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _status_platform_key_allowed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_status_platform_key_allowed");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_status_platform_key_allowed");
    }
    return 0;
}
