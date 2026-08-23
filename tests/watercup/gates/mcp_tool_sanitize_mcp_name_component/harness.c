/* gate: mcp_tool_sanitize_mcp_name_component — traced copy of
 * src/tools/mcp_tool.c::mcp_tool_sanitize_mcp_name_component.
 * LIVE twin: tools/mcp_tool.py::sanitize_mcp_name_component.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: null name -> "unknown"
 *   LOOP over each char: keep [A-Za-z0-9_] else '_'
 * KNOWN DIVERGENCE (excluded input): LIVE also maps '-' to '_', the C port
 * preserves it. Hyphen-bearing inputs are not exercised.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

static char *wc_sanitize(const char *name) {
    WCUP_BEGIN("sanitize_mcp_name_component");
    char *result;
    if (!name) {                                     /* B1 */
        WCUP_BRANCH(1);
        result = strdup("unknown");
        goto done;
    }
    WCUP_BRANCH(0);
    {
        size_t len = strlen(name);
        result = malloc(len + 1);
        for (size_t i = 0; i < len; i++) {           /* LOOP chars */
            WCUP_LOOP_TICK();
            char c = name[i];
            bool keep = isalnum((unsigned char)c) || c == '_';
            result[i] = keep ? c : '_';
        }
        result[len] = '\0';
    }
done: {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sanitize_mcp_name_component");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sanitize_mcp_name_component <name|NULL>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        wc_sanitize(strcmp(argv[i], "NULL") == 0 ? NULL : argv[i]);
    return 0;
}
