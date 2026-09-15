/* gate: agent_models_dev_py___save_disk_cache — traced copy of mdv_save_disk_cache */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mdv_save_disk_cache(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("mdv_save_disk_cache");
    int result = mdv_save_disk_cache(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mdv_save_disk_cache");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mdv_save_disk_cache <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
