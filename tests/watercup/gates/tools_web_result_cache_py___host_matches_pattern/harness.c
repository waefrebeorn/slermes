/* gate: tools_web_result_cache_py___host_matches_pattern — traced copy of _host_matches_pattern */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _host_matches_pattern(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("_host_matches_pattern");
    int result = _host_matches_pattern(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_host_matches_pattern");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _host_matches_pattern <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
