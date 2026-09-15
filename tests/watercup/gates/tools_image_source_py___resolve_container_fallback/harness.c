/* gate: tools_image_source_py___resolve_container_fallback — traced copy of tools_image_source_u_resolve_container_fallback */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_image_source_u_resolve_container_fallback(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_image_source_u_resolve_container_fallback");
    int result = tools_image_source_u_resolve_container_fallback(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_image_source_u_resolve_container_fallback");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_image_source_u_resolve_container_fallback <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
