/* gate: tools_send_message_tool_py___resolve_slack_user_target — traced copy of tools_send_message_tool_u_resolve_slack_user_target */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_send_message_tool_u_resolve_slack_user_target(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_send_message_tool_u_resolve_slack_user_target");
    int result = tools_send_message_tool_u_resolve_slack_user_target(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_send_message_tool_u_resolve_slack_user_target");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_send_message_tool_u_resolve_slack_user_target <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
