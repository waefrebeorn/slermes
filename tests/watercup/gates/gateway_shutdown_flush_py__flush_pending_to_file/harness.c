/* gate: gateway_shutdown_flush_py__flush_pending_to_file — traced copy of shutdown_flush_pending_to_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int shutdown_flush_pending_to_file(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("shutdown_flush_pending_to_file");
    int result = shutdown_flush_pending_to_file(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("shutdown_flush_pending_to_file");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s shutdown_flush_pending_to_file <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
