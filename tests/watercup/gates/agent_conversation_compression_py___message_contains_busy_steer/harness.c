/* gate: {'name': 'agent_conversation_compression_py___message_contains_busy_steer', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_message_contains_busy_steer', 'py_module': 'agent/conversation_compression.py', 'py_fn': '_message_contains_busy_steer', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src']} — GAP sentinel for _message_contains_busy_steer */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_message_contains_busy_steer";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _message_contains_busy_steer <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_message_contains_busy_steer");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_message_contains_busy_steer");
    }
    return 0;
}
