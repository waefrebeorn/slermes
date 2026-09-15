/* gate: tools_environments_vercel_sandbox_py___store_snapshot — traced copy of ve_store_snapshot */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ve_store_snapshot(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("ve_store_snapshot");
    int result = ve_store_snapshot(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ve_store_snapshot");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ve_store_snapshot <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
