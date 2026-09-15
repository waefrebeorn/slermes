/* gate: agent_lsp_manager_py___idle_reaper_loop — traced copy of lsp_service_idle_reaper_loop */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lsp_service_idle_reaper_loop(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("lsp_service_idle_reaper_loop");
    int result = lsp_service_idle_reaper_loop(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lsp_service_idle_reaper_loop");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lsp_service_idle_reaper_loop <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
