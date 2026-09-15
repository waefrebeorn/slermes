/* gate: gateway_platforms_base_py__delete_message — traced copy of pb_delete_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pb_delete_message(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("pb_delete_message");
    int result = pb_delete_message(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pb_delete_message");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pb_delete_message <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
