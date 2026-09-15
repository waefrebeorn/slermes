/* gate: tools_homeassistant_tool_py___parse_service_response — traced copy of tools_homeassistant_tool_u_parse_service_response */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_homeassistant_tool_u_parse_service_response(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_homeassistant_tool_u_parse_service_response");
    int result = tools_homeassistant_tool_u_parse_service_response(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_homeassistant_tool_u_parse_service_response");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_homeassistant_tool_u_parse_service_response <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
