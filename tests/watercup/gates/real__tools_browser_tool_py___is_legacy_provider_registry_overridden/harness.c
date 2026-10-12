/* gate: real__tools_browser_tool_py___is_legacy_provider_registry_overridden — GAP sentinel for _is_legacy_provider_registry_overridden */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_is_legacy_provider_registry_overridden";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _is_legacy_provider_registry_overridden <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_is_legacy_provider_registry_overridden");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_is_legacy_provider_registry_overridden");
    }
    return 0;
}
