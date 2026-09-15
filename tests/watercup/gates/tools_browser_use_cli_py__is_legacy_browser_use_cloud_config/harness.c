/* gate: tools_browser_use_cli_py__is_legacy_browser_use_cloud_config — traced copy of bu_is_legacy_browser_use_cloud_config */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int bu_is_legacy_browser_use_cloud_config(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("bu_is_legacy_browser_use_cloud_config");
    int result = bu_is_legacy_browser_use_cloud_config(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bu_is_legacy_browser_use_cloud_config");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bu_is_legacy_browser_use_cloud_config <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
