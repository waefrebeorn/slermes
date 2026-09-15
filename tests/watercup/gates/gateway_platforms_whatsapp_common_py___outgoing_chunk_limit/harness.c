/* gate: gateway_platforms_whatsapp_common_py___outgoing_chunk_limit — traced copy of whatsapp_common_outgoing_chunk_limit */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int whatsapp_common_outgoing_chunk_limit(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("whatsapp_common_outgoing_chunk_limit");
    int result = whatsapp_common_outgoing_chunk_limit(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("whatsapp_common_outgoing_chunk_limit");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s whatsapp_common_outgoing_chunk_limit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
