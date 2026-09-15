/* gate: gateway_session_py___get_or_create_session_impl — traced copy of _get_or_create_session_impl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _get_or_create_session_impl(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_get_or_create_session_impl");
    int result = _get_or_create_session_impl(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_get_or_create_session_impl");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _get_or_create_session_impl <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
