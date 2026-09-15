/* gate: hermes_cli__scan_venv_blockers_py___emit_probe_fail — traced copy of scanvb_emit_probe_fail */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int scanvb_emit_probe_fail(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("scanvb_emit_probe_fail");
    int result = scanvb_emit_probe_fail(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("scanvb_emit_probe_fail");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s scanvb_emit_probe_fail <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
