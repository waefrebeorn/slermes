/* gate: agent_lsp_eventlog_py__log_spawn_failed — traced copy of lel_log_spawn_failed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lel_log_spawn_failed(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("lel_log_spawn_failed");
    int result = lel_log_spawn_failed(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lel_log_spawn_failed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lel_log_spawn_failed <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
