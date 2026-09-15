/* gate: hermes_cli_models_py___cache_entry_valid — traced copy of models_cache_entry_valid */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int models_cache_entry_valid(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("models_cache_entry_valid");
    int result = models_cache_entry_valid(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("models_cache_entry_valid");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_cache_entry_valid <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
