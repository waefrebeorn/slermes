/* gate: agent_memory_manager_py__notify_memory_tool_write — traced copy of mm_notify_memory_tool_write */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mm_notify_memory_tool_write(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mm_notify_memory_tool_write");
    int result = mm_notify_memory_tool_write(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mm_notify_memory_tool_write");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mm_notify_memory_tool_write <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
