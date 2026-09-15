/* gate: gateway_platforms_whatsapp_common_py___is_broadcast_chat — traced copy of whatsapp_common_is_broadcast_chat */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool whatsapp_common_is_broadcast_chat(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("whatsapp_common_is_broadcast_chat");
    bool result = whatsapp_common_is_broadcast_chat(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("whatsapp_common_is_broadcast_chat");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s whatsapp_common_is_broadcast_chat <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
