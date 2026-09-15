/* gate: hermes_cli_model_setup_flows_py___model_flow_azure_foundry — traced copy of msf_u_model_flow_azure_foundry */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msf_u_model_flow_azure_foundry(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msf_u_model_flow_azure_foundry");
    int result = msf_u_model_flow_azure_foundry(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msf_u_model_flow_azure_foundry");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msf_u_model_flow_azure_foundry <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
