/* t_port_agent_intent_ack.c — oracle harness for
 * agent/agent_runtime_helpers.py:intent_ack_continuation_mode
 * Emits {"mode":..,"api_mode":..,"model":..,"out":<mode-string>}. */
#include <stdio.h>
#include "cli/port_agent_intent_ack.h"

static const char *js(const char *s) {
    static char bufs[8][512];
    static int bi = 0;
    char *b = bufs[bi];
    bi = (bi + 1) % 8;
    char *q = b;
    *q++ = '"';
    for (const char *p = s; p && *p && q - b < 500; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

static void emit(const char *mode, const char *api, const char *model) {
    char *r = intent_ack_continuation_mode(mode, api, model);
    printf("{\"mode\":%s,\"api_mode\":%s,\"model\":%s,\"out\":%s}\n",
           js(mode), js(api), js(model), js(r ? r : ""));
    free(r);
}

int main(void) {
    emit("auto", "codex_responses", "gpt-4o");
    emit("auto", "openai_chat", "gpt-4o");
    emit("true", "openai_chat", "gpt-4o");
    emit("always", "anything", "x");
    emit("off", "codex_responses", "x");
    emit("never", "codex_responses", "x");
    emit("on", "openai_chat", "x");
    emit("codex;claude", "openai_chat", "anthropic/claude-opus-4");
    emit("codex;grok", "openai_chat", "openai/gpt-4o");
    emit("wat", "codex_responses", "x");
    return 0;
}
