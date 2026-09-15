/* gate: agent_kanban_stop_py__session_called_kanban_terminal — traced copy of agent_kanban_stop_session_called_kanban_terminal */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_kanban_stop_session_called_kanban_terminal(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_kanban_stop_session_called_kanban_terminal");
    int result = agent_kanban_stop_session_called_kanban_terminal(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_kanban_stop_session_called_kanban_terminal");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_kanban_stop_session_called_kanban_terminal <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
