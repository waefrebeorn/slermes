/* gate: hermes_cli_plugins_cmd_py___save_memory_provider — traced copy of pcmd_save_memory_provider */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int pcmd_save_memory_provider(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("pcmd_save_memory_provider");
    int result = pcmd_save_memory_provider(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pcmd_save_memory_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pcmd_save_memory_provider <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
