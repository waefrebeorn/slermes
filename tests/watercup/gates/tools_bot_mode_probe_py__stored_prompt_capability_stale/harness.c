/* gate: tools_bot_mode_probe_py__stored_prompt_capability_stale — traced copy of stored_prompt_capability_stale */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int stored_prompt_capability_stale(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("stored_prompt_capability_stale");
    int result = stored_prompt_capability_stale(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("stored_prompt_capability_stale");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stored_prompt_capability_stale <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
