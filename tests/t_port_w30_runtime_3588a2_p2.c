/* JSON-lines harness for the w30_runtime_3588a2_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_runtime_3588a2_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "idle_stop_seconds"))       return w30_rt_p2_idle_stop_seconds(in);
    if (!strcmp(func, "idle_seconds"))            return w30_rt_p2_idle_seconds(in);
    if (!strcmp(func, "idle_due"))                return w30_rt_p2_idle_due(in);
    if (!strcmp(func, "should_auto_start"))       return w30_rt_p2_should_auto_start(in);
    if (!strcmp(func, "tool_placement"))          return w30_rt_p2_tool_placement(in);
    if (!strcmp(func, "ensure_started_for_tool")) return w30_rt_p2_ensure_started_for_tool(in);
    if (!strcmp(func, "start"))                   return w30_rt_p2_start(in);
    if (!strcmp(func, "status"))                  return w30_rt_p2_status(in);
    if (!strcmp(func, "sandbox_status"))          return w30_rt_p2_sandbox_status(in);
    if (!strcmp(func, "stop"))                    return w30_rt_p2_stop(in);
    if (!strcmp(func, "in_sandbox"))              return w30_rt_p2_in_sandbox(in);
    return json_string("UNKNOWN_FUNC");
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        out = dispatch(in, func);
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
