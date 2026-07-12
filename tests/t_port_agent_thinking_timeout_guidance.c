/* t_port_agent_thinking_timeout_guidance.c — oracle harness for
 * agent/thinking_timeout_guidance.py:is_thinking_timeout
 * Emits {"reason":..,"model":..,"err":..,"out":<bool>}. */
#include <stdio.h>
#include "cli/port_agent_thinking_timeout_guidance.h"

static const char *js(const char *s) {
    static char bufs[8][4096];
    static int bi = 0;
    char *b = bufs[bi];
    bi = (bi + 1) % 8;
    char *q = b;
    *q++ = '"';
    for (const char *p = s; p && *p && q - b < 4000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

static void emit(const char *reason, const char *model, const char *err) {
    int r = thinking_timeout_is(reason, model, err);
    printf("{\"reason\":%s,\"model\":%s,\"err\":%s,\"out\":%s}\n",
           js(reason), js(model), js(err), r ? "true" : "false");
}

int main(void) {
    emit("timeout", "nvidia/nemotron-3-ultra-550b-a55b", "broken pipe from upstream");
    emit("timeout", "openai/o3-mini", "remote protocol error");
    emit("auth", "openai/o3-mini", "broken pipe");
    emit("timeout", "gpt-4o", "broken pipe");
    emit("timeout", "nvidia/nemotron-3-ultra", "rate limited 429");
    emit("timeout", "x-ai/grok-4-fast-reasoning", "errno 32");
    emit("timeout", "deepseek/deepseek-r1", "connection reset by peer");
    emit("timeout", "anthropic/claude-opus-4-6", "all good");
    return 0;
}
