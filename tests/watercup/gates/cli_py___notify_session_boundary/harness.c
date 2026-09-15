/* gate: cli_py___notify_session_boundary — traced copy of cl2_notify_session_boundary */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cl2_notify_session_boundary(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cl2_notify_session_boundary");
    int result = cl2_notify_session_boundary(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cl2_notify_session_boundary");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cl2_notify_session_boundary <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
