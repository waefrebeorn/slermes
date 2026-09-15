/* gate: api_server_py__body_limit_middleware — traced copy of apisg_body_limit_middleware */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int apisg_body_limit_middleware(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("apisg_body_limit_middleware");
    int result = apisg_body_limit_middleware(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apisg_body_limit_middleware");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apisg_body_limit_middleware <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
