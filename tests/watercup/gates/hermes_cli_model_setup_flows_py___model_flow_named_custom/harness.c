/* gate: hermes_cli_model_setup_flows_py___model_flow_named_custom — traced copy of msf_u_model_flow_named_custom */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msf_u_model_flow_named_custom(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msf_u_model_flow_named_custom");
    int result = msf_u_model_flow_named_custom(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msf_u_model_flow_named_custom");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msf_u_model_flow_named_custom <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
