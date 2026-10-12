/* gate: real__agent_proxy_sources_iron_proxy_py___http_download — GAP sentinel for _http_download */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_http_download";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _http_download <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_http_download");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_http_download");
    }
    return 0;
}
