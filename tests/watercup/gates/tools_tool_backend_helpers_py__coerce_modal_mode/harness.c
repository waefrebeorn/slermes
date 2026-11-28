/* gate: tools_tool_backend_helpers_py__coerce_modal_mode */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *tools_tool_backend_helpers_coerce_modal_mode(const char *value);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <value>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tools_tool_backend_helpers_coerce_modal_mode");
        fflush(stdout);
        char *r = tools_tool_backend_helpers_coerce_modal_mode(argv[i]);
        WCUP_VALSTR(r ? r : "");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("tools_tool_backend_helpers_coerce_modal_mode");
        free(r);
    }
    return 0;
}
