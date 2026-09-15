/* gate: tools_daemon_pool_py___adjust_thread_count — traced copy of tools_daemon_pool_u_adjust_thread_count */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_daemon_pool_u_adjust_thread_count(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_daemon_pool_u_adjust_thread_count");
    int result = tools_daemon_pool_u_adjust_thread_count(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_daemon_pool_u_adjust_thread_count");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_daemon_pool_u_adjust_thread_count <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
