/* gate: agent_web_search_provider_py__supports_extract — traced copy of cli_agent_web_search_provider_supports_extract */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_agent_web_search_provider_supports_extract(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_agent_web_search_provider_supports_extract");
    int result = cli_agent_web_search_provider_supports_extract(input);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_agent_web_search_provider_supports_extract");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_agent_web_search_provider_supports_extract <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
