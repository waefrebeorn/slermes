/* gate: hermes_cli_skin_engine_py__get_prompt_toolkit_style_overrides — traced copy of hermes_cli_skin_engine_get_prompt_toolkit_style_overrides */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_skin_engine_get_prompt_toolkit_style_overrides(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_skin_engine_get_prompt_toolkit_style_overrides");
    int result = hermes_cli_skin_engine_get_prompt_toolkit_style_overrides(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_skin_engine_get_prompt_toolkit_style_overrides");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_skin_engine_get_prompt_toolkit_style_overrides <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
