/* gate: gateway_relay_adapter_py___get_media_client — traced copy of relay_helper_get_media_client */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int relay_helper_get_media_client(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("relay_helper_get_media_client");
    int result = relay_helper_get_media_client(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("relay_helper_get_media_client");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s relay_helper_get_media_client <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
