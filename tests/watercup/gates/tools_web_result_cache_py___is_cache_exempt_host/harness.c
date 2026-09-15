/* gate: tools_web_result_cache_py___is_cache_exempt_host — traced copy of _is_cache_exempt_host */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _is_cache_exempt_host(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_is_cache_exempt_host");
    int result = _is_cache_exempt_host(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_is_cache_exempt_host");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_cache_exempt_host <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
