/* gate: real__agent_transports_codex_app_server_session_py__compact_thread — GAP sentinel for compact_thread */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "compact_thread";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s compact_thread <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("compact_thread");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("compact_thread");
    }
    return 0;
}
