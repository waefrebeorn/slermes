/* gate: real__hermes_cli_plugins_py__has_plugin — GAP sentinel for has_plugin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "has_plugin";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s has_plugin <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("has_plugin");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("has_plugin");
    }
    return 0;
}
