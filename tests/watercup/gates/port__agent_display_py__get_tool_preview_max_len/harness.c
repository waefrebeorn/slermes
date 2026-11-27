/* gate: {'name': 'port__agent_display_py__get_tool_preview_max_len', 'subsystem': 'auto', 'kind': 'cup', 'c_src': 'harness.c', 'c_fn': 'length', 'py_module': 'agent/display.py', 'py_fn': 'get_tool_preview_max_len', 'inputs': ['hello', 'world', '', 'test123', 'NULL'], 'extra_src': ['lib/libjson/json.c', 'src/agent/logger.c', 'src/cli/paths.c', 'src/hermes_cli/fs_replace.c', 'src/agent/display_core.c'], 'cflags': ['-I', 'include', '-I', 'lib', '-I', 'lib/libjson', '-I', 'src'], 'classification': 'PORTED'} — GAP sentinel for get_tool_preview_max_len */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "get_tool_preview_max_len";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s get_tool_preview_max_len <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("get_tool_preview_max_len");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("get_tool_preview_max_len");
    }
    return 0;
}
