/* gate: hermes_cli_model_setup_flows_py___model_flow_api_key_provider — traced copy of hcli_model_flow_api_key */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hcli_model_flow_api_key(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("hcli_model_flow_api_key");
    int result = hcli_model_flow_api_key(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hcli_model_flow_api_key");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hcli_model_flow_api_key <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
