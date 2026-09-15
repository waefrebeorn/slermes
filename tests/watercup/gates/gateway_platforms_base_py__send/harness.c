/* gate: gateway_platforms_base_py__send — traced copy of pb_send */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pb_send(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("pb_send");
    int result = pb_send(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pb_send");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pb_send <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
