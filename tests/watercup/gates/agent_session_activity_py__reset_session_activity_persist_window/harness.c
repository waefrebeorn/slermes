/* gate: agent_session_activity_py__reset_session_activity_persist_window — traced copy of sa_reset_session_activity_persist_window */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sa_reset_session_activity_persist_window(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sa_reset_session_activity_persist_window");
    int result = sa_reset_session_activity_persist_window(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sa_reset_session_activity_persist_window");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sa_reset_session_activity_persist_window <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
