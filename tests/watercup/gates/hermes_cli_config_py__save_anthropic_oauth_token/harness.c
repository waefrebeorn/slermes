/* gate: hermes_cli_config_py__save_anthropic_oauth_token — traced copy of cfg_save_anthropic_oauth_token */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cfg_save_anthropic_oauth_token(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cfg_save_anthropic_oauth_token");
    int result = cfg_save_anthropic_oauth_token(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cfg_save_anthropic_oauth_token");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cfg_save_anthropic_oauth_token <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
