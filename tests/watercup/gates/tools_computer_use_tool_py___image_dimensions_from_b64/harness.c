/* gate: tools_computer_use_tool_py___image_dimensions_from_b64 — traced copy of tools_computer_use_tool_u_image_dimensions_from_b64 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_computer_use_tool_u_image_dimensions_from_b64(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_computer_use_tool_u_image_dimensions_from_b64");
    int result = tools_computer_use_tool_u_image_dimensions_from_b64(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_computer_use_tool_u_image_dimensions_from_b64");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_computer_use_tool_u_image_dimensions_from_b64 <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
