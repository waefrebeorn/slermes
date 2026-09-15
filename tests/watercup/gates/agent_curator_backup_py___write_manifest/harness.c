/* gate: agent_curator_backup_py___write_manifest — traced copy of cub_write_manifest */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cub_write_manifest(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cub_write_manifest");
    int result = cub_write_manifest(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cub_write_manifest");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cub_write_manifest <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
