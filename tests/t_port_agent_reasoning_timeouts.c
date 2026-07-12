/* t_port_agent_reasoning_timeouts.c — oracle harness for
 * agent/reasoning_timeouts.py:get_reasoning_stale_timeout_floor
 *
 * Emits one JSON object per line: {"in":"<model>","out":<floor-or-null>}
 * where out is the numeric floor (e.g. 600.0) or null when the Python
 * side returns None. The oracle replays the SAME inputs through the live
 * Python module and compares exactly. */
#include <stdio.h>
#include <stdlib.h>
#include "cli/port_agent_reasoning_timeouts.h"

/* JSON-encode a string for the oracle input. */
static const char *js(const char *s) {
    static char b[4096];
    char *q = b;
    *q++ = '"';
    for (const char *p = s; *p && q - b < 4000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else if (c == '\t') { *q++ = '\\'; *q++ = 't'; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

static void emit(const char *model) {
    double f = reasoning_timeouts_get_floor(model);
    if (f < 0) {
        printf("{\"in\":%s,\"out\":null}\n", js(model));
    } else {
        printf("{\"in\":%s,\"out\":%g}\n", js(model), f);
    }
}

int main(void) {
    emit("nvidia/nemotron-3-ultra-550b-a55b");
    emit("openai/o3-mini");
    emit("deepseek/deepseek-r1");
    emit("qwen/qwen3-235b-a22b-thinking");
    emit("x-ai/grok-4-fast-reasoning");
    emit("anthropic/claude-opus-4-6");
    emit("gpt-4o");
    emit("olmo-1");
    emit("openai/o3-mini-fork");
    emit("some-other-qwen3");
    emit("llama-4-70b-o1-preview");
    emit("");
    emit("none");
    emit("NVIDIA/Nemotron-3-Ultra");
    emit("OpenAI/O3-Mini");
    emit("qwen3-235b-instruct");
    emit("claude-opus-4-thinking");
    return 0;
}
