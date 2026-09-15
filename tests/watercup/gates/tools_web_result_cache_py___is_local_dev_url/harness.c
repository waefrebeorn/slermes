/* gate: tools_web_result_cache_py___is_local_dev_url — traced copy of _is_local_dev_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _is_local_dev_url(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_is_local_dev_url");
    int result = _is_local_dev_url(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_is_local_dev_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_local_dev_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
