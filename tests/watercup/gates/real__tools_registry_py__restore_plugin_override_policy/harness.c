/* gate: real__tools_registry_py__restore_plugin_override_policy — GAP sentinel for restore_plugin_override_policy */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "restore_plugin_override_policy";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s restore_plugin_override_policy <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("restore_plugin_override_policy");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("restore_plugin_override_policy");
    }
    return 0;
}
