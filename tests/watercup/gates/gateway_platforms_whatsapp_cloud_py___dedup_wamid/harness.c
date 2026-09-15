/* gate: gateway_platforms_whatsapp_cloud_py___dedup_wamid — traced copy of whatsapp_cloud_dedup_wamid */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool whatsapp_cloud_dedup_wamid(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("whatsapp_cloud_dedup_wamid");
    bool result = whatsapp_cloud_dedup_wamid(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("whatsapp_cloud_dedup_wamid");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s whatsapp_cloud_dedup_wamid <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
