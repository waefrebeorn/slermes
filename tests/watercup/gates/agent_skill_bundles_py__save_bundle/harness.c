/* gate: agent_skill_bundles_py__save_bundle — traced copy of sbd_save_bundle */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sbd_save_bundle(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("sbd_save_bundle");
    int result = sbd_save_bundle(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sbd_save_bundle");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sbd_save_bundle <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
