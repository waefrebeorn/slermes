/* gate: agent_chat_completion_helpers_py__cleanup_task_resources — traced copy of cch_cleanup_task_resources */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cch_cleanup_task_resources(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cch_cleanup_task_resources");
    int result = cch_cleanup_task_resources(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cch_cleanup_task_resources");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cch_cleanup_task_resources <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
