/* gate: gateway_platforms_yuanbao_py___is_self_reference — traced copy of yb_u_is_self_reference */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_u_is_self_reference(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("yb_u_is_self_reference");
    int result = yb_u_is_self_reference(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_u_is_self_reference");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_u_is_self_reference <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
