/* gate: tools_computer_use_cua_backend_py__stop_recording — traced copy of cua_stop_recording */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cua_stop_recording(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cua_stop_recording");
    int result = cua_stop_recording(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cua_stop_recording");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cua_stop_recording <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
