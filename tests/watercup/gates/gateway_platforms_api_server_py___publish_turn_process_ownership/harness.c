/* gate: gateway_platforms_api_server_py___publish_turn_process_ownership — traced copy of gwapi_publish_turn_process_ownership */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gwapi_publish_turn_process_ownership(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gwapi_publish_turn_process_ownership");
    int result = gwapi_publish_turn_process_ownership(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gwapi_publish_turn_process_ownership");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gwapi_publish_turn_process_ownership <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
