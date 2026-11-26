/* gate: agent_lsp_eventlog_py___announce_once — traced copy of lel_announce_once */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lel_announce_once(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("lel_announce_once");
    int result = lel_announce_once(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lel_announce_once");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lel_announce_once <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
