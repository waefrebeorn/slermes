/* gate: gateway_pairing_py___platform_uses_whatsapp_identity — traced copy of platform_uses_whatsapp_identity */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool platform_uses_whatsapp_identity(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("platform_uses_whatsapp_identity");
    bool result = platform_uses_whatsapp_identity(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("platform_uses_whatsapp_identity");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s platform_uses_whatsapp_identity <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
