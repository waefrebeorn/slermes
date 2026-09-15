/* gate: tools_kanban_tools_py___handle_attachments — traced copy of tools_kanban_tools_u_handle_attachments */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_kanban_tools_u_handle_attachments(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_kanban_tools_u_handle_attachments");
    int result = tools_kanban_tools_u_handle_attachments(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_kanban_tools_u_handle_attachments");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_kanban_tools_u_handle_attachments <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
