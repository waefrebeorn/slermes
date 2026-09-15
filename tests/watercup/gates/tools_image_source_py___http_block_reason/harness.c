/* gate: tools_image_source_py___http_block_reason — traced copy of tools_image_source_u_http_block_reason */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_image_source_u_http_block_reason(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_image_source_u_http_block_reason");
    int result = tools_image_source_u_http_block_reason(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_image_source_u_http_block_reason");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_image_source_u_http_block_reason <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
