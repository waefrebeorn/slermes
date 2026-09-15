/* gate: gateway_channel_directory_py___slack_api_error_code — traced copy of gateway_channel_directory_u_slack_api_error_code */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_channel_directory_u_slack_api_error_code(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_channel_directory_u_slack_api_error_code");
    int result = gateway_channel_directory_u_slack_api_error_code(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_channel_directory_u_slack_api_error_code");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_channel_directory_u_slack_api_error_code <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
