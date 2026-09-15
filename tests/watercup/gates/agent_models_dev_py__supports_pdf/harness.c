/* gate: agent_models_dev_py__supports_pdf — traced copy of mdv_supports_pdf */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mdv_supports_pdf(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mdv_supports_pdf");
    bool result = mdv_supports_pdf(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mdv_supports_pdf");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mdv_supports_pdf <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
