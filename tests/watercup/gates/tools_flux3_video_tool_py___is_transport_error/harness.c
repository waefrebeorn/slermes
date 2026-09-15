/* gate: tools_flux3_video_tool_py___is_transport_error — traced copy of f3_is_transport_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool f3_is_transport_error(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("f3_is_transport_error");
    bool result = f3_is_transport_error(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("f3_is_transport_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s f3_is_transport_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
