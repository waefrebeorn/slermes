/* gate: hermes_cli__scan_venv_blockers_py__main — traced copy of scanvb_main */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int scanvb_main(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("scanvb_main");
    int result = scanvb_main(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("scanvb_main");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s scanvb_main <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
