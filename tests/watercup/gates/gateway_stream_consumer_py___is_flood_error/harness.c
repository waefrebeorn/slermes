/* gate: {'name': 'gateway_stream_consumer_py___is_flood_error', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_is_flood_error', 'py_module': 'gateway/stream_consumer.py', 'py_fn': '_is_flood_error', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src']} — GAP sentinel for _is_flood_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_is_flood_error";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_flood_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_is_flood_error");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_is_flood_error");
    }
    return 0;
}
