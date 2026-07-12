/* t_port_agent_retry_utils.c — oracle harness for
 * agent/retry_utils.py:is_zai_coding_overload_error
 * Emits {"in":<json-describing-error>,"out":<bool>}. */
#include <stdio.h>
#include "cli/port_agent_retry_utils.h"

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
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else if (c == '\t') { *q++ = '\\'; *q++ = 't'; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

static void emit(const char *base, const char *model, int status, const char *text) {
    retry_error_t e;
    e.status_code = status;
    e.text = text;
    int r = retry_utils_is_zai_overload(base, model, &e);
    printf("{\"base\":%s,\"model\":%s,\"status\":%d,\"text\":%s,\"out\":%s}\n",
           js(base), js(model), status, js(text ? text : ""), r ? "true" : "false");
}

int main(void) {
    emit("https://api.z.ai/api/coding/paas/v4", "glm-5.2", 429,
         "The service may be temporarily overloaded (code 1305)");
    emit("https://api.z.ai/api/coding/paas/v4", "glm-5.2", 429, "quota exceeded");
    emit("https://api.z.ai/api/coding/paas/v4", "glm-5.2", 200, "ok");
    emit("https://api.openai.com/v1", "glm-5.2", 429, "temporarily overloaded 1305");
    emit("https://api.z.ai/api/coding/paas/v4", "gpt-4o", 429, "temporarily overloaded 1305");
    emit("https://api.z.ai/api/coding/paas/v4", "glm-5.2", 429, "rate limited");
    emit("other", "glm-5.2", 429, "temporarily overloaded");
    return 0;
}
