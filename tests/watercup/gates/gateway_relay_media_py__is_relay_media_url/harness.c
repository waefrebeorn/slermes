/* gate: gateway_relay_media_py__is_relay_media_url — traced copy of relay_media_client_is_relay_media_url */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool relay_media_client_is_relay_media_url(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("relay_media_client_is_relay_media_url");
    bool result = relay_media_client_is_relay_media_url(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("relay_media_client_is_relay_media_url");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s relay_media_client_is_relay_media_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
