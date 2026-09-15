/* gate: hermes_cli_runtime_provider_py___anthropic_base_url_override_ok — traced copy of provider_anthropic_base_url_override_ok */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool provider_anthropic_base_url_override_ok(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("provider_anthropic_base_url_override_ok");
    bool result = provider_anthropic_base_url_override_ok(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("provider_anthropic_base_url_override_ok");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s provider_anthropic_base_url_override_ok <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
