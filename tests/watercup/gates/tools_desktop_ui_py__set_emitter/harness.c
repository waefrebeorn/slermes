/* gate: tools_desktop_ui_py__set_emitter — traced copy of tools_desktop_ui_set_emitter */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_desktop_ui_set_emitter(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_desktop_ui_set_emitter");
    int result = tools_desktop_ui_set_emitter(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_desktop_ui_set_emitter");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_desktop_ui_set_emitter <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
