/* gate: tools_computer_use_cua_backend_py__get_recording_state — traced copy of cua_get_recording_state */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cua_get_recording_state(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cua_get_recording_state");
    int result = cua_get_recording_state(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cua_get_recording_state");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cua_get_recording_state <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
