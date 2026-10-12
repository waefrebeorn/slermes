/* gate: real__tools_plugin_guard_py__should_allow_plugin_install — GAP sentinel for should_allow_plugin_install */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "should_allow_plugin_install";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s should_allow_plugin_install <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("should_allow_plugin_install");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("should_allow_plugin_install");
    }
    return 0;
}
