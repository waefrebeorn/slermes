/* gate: real__tools_session_search_tool_py___is_compaction_summary -> real is_compaction_summary (int 1/0). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern int tools_session_search_tool_u_is_compaction_summary(const char *content);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <content>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tools_session_search_tool_u_is_compaction_summary");
        fflush(stdout);
        int r = tools_session_search_tool_u_is_compaction_summary(argv[i]);
        WCUP_VALINT(r);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("tools_session_search_tool_u_is_compaction_summary");
    }
    return 0;
}
