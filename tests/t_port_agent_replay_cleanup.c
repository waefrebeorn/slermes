/* t_port_agent_replay_cleanup.c — oracle harness for
 * agent/replay_cleanup.py:is_interrupted_tool_result
 *
 * Emits one JSON object per line: {"in":"<text>","out":<bool>}.
 * The oracle replays the SAME inputs through the live Python function. */
#include <stdio.h>
#include "cli/port_agent_replay_cleanup.h"

static const char *js(const char *s) {
    static char bufs[8][8192];
    static int bi = 0;
    char *b = bufs[bi];
    bi = (bi + 1) % 8;
    char *q = b;
    *q++ = '"';
    for (const char *p = s; p && *p && q - b < 8000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

static void emit(const char *text) {
    int r = replay_cleanup_is_interrupted(text);
    printf("{\"in\":%s,\"out\":%s}\n", js(text), r ? "true" : "false");
}

int main(void) {
    emit("The tool finished cleanly with output.");
    emit("Error: [command interrupted] by user");
    emit("Process exited with exit_code 130 and interrupt received");
    emit("exit_code -1 interrupt signal handled");
    emit("exit_code 130 but no interrupt keyword here");
    emit("exit_code 0 completed normally");
    emit("{\"exit_code\": 130, \"note\": \"interrupt during shutdown\"}");
    emit(NULL);
    emit("");
    emit("interrupted but no exit_code field present");
    return 0;
}
