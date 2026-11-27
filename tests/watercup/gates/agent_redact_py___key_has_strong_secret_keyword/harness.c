/* gate: {'name': 'agent_redact_py___key_has_strong_secret_keyword', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': '_key_has_strong_secret_keyword', 'py_module': 'agent/redact.py', 'py_fn': '_key_has_strong_secret_keyword', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src']} — GAP sentinel for _key_has_strong_secret_keyword */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_key_has_strong_secret_keyword";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _key_has_strong_secret_keyword <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_key_has_strong_secret_keyword");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_key_has_strong_secret_keyword");
    }
    return 0;
}
