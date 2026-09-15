/* gate: gateway_platform_registry_py__register — traced copy of prg_register */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int prg_register(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("prg_register");
    int result = prg_register(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("prg_register");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s prg_register <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
