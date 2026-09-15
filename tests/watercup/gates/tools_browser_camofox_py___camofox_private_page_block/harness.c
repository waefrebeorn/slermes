/* gate: tools_browser_camofox_py___camofox_private_page_block — traced copy of _camofox_private_page_block */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool _camofox_private_page_block(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_camofox_private_page_block");
    bool result = _camofox_private_page_block(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_camofox_private_page_block");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _camofox_private_page_block <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
