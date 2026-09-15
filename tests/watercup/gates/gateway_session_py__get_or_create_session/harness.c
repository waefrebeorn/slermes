/* gate: gateway_session_py__get_or_create_session — traced copy of get_or_create_session */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int get_or_create_session(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("get_or_create_session");
    int result = get_or_create_session(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("get_or_create_session");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s get_or_create_session <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
