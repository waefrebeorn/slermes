/* gate: agent_transports_codex_app_server_py__notify — traced copy of cas_notify */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cas_notify(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cas_notify");
    int result = cas_notify(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cas_notify");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cas_notify <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
