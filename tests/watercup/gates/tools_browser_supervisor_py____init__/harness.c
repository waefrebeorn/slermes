/* gate: tools_browser_supervisor_py____init__ — traced copy of bsv_init */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int bsv_init(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("bsv_init");
    int result = bsv_init(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bsv_init");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bsv_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
