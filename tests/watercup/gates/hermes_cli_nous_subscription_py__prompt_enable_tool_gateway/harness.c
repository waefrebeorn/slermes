/* gate: hermes_cli_nous_subscription_py__prompt_enable_tool_gateway — traced copy of nsub_prompt_enable_tool_gateway */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int nsub_prompt_enable_tool_gateway(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("nsub_prompt_enable_tool_gateway");
    int result = nsub_prompt_enable_tool_gateway(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("nsub_prompt_enable_tool_gateway");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nsub_prompt_enable_tool_gateway <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
