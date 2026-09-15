/* gate: hermes_cli_model_setup_flows_py___prompt_auth_credentials_choice — traced copy of msf_u_prompt_auth_credentials_choice */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int msf_u_prompt_auth_credentials_choice(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("msf_u_prompt_auth_credentials_choice");
    int result = msf_u_prompt_auth_credentials_choice(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("msf_u_prompt_auth_credentials_choice");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s msf_u_prompt_auth_credentials_choice <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
