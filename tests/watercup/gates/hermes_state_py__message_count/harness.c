/* gate: hermes_state_py__message_count — traced copy of ws_sessions_message_count */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ws_sessions_message_count(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("ws_sessions_message_count");
    int result = ws_sessions_message_count(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_sessions_message_count");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_sessions_message_count <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
