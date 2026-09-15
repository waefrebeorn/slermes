/* gate: gateway_platforms_base_py___media_delivery_strict_mode — traced copy of pb_media_delivery_strict_mode */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool pb_media_delivery_strict_mode(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("pb_media_delivery_strict_mode");
    bool result = pb_media_delivery_strict_mode(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("pb_media_delivery_strict_mode");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pb_media_delivery_strict_mode <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
