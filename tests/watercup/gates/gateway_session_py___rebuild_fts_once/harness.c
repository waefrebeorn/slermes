/* gate: gateway_session_py___rebuild_fts_once — traced copy of _rebuild_fts_once */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _rebuild_fts_once(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_rebuild_fts_once");
    int result = _rebuild_fts_once(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_rebuild_fts_once");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _rebuild_fts_once <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
