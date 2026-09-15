/* gate: tools_kanban_tools_py___download_url_with_cap — traced copy of tools_kanban_tools_u_download_url_with_cap */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_kanban_tools_u_download_url_with_cap(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_kanban_tools_u_download_url_with_cap");
    int result = tools_kanban_tools_u_download_url_with_cap(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_kanban_tools_u_download_url_with_cap");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_kanban_tools_u_download_url_with_cap <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
