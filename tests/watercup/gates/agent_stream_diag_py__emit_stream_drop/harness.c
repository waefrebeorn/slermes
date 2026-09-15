/* gate: agent_stream_diag_py__emit_stream_drop — traced copy of sdi_emit_stream_drop */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sdi_emit_stream_drop(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sdi_emit_stream_drop");
    int result = sdi_emit_stream_drop(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sdi_emit_stream_drop");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sdi_emit_stream_drop <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
