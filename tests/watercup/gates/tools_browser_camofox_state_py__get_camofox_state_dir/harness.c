/* gate: tools_browser_camofox_state_py__get_camofox_state_dir — traced copy of tools_browser_camofox_state_get_camofox_state_dir */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_browser_camofox_state_get_camofox_state_dir(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_browser_camofox_state_get_camofox_state_dir");
    int result = tools_browser_camofox_state_get_camofox_state_dir(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_browser_camofox_state_get_camofox_state_dir");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_browser_camofox_state_get_camofox_state_dir <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
