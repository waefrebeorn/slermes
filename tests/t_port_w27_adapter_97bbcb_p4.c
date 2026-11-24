/* JSON-lines harness for the w27_adapter_97bbcb_p4 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_97bbcb_p4.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "format_message")) {
            out = w27_gchat_p4_format_message(in);
        } else if (!strcmp(func, "retry_plan")) {
            out = w27_gchat_p4_retry_plan(in);
        } else if (!strcmp(func, "reconnect_delay")) {
            out = w27_gchat_p4_reconnect_delay(in);
        } else if (!strcmp(func, "reconnect_jitter")) {
            out = w27_gchat_p4_reconnect_jitter(in);
        } else if (!strcmp(func, "bot_spaces")) {
            out = w27_gchat_p4_bot_spaces(in);
        } else if (!strcmp(func, "sa_source_kind")) {
            out = w27_gchat_p4_sa_source_kind(in);
        } else if (!strcmp(func, "release_unadmitted")) {
            out = w27_gchat_p4_release_unadmitted(in);
        } else if (!strcmp(func, "outbound_thread_incr")) {
            out = w27_gchat_p4_outbound_thread_incr(in);
        } else if (!strcmp(func, "invalidate_identity")) {
            out = w27_gchat_p4_invalidate_identity(in);
        } else if (!strcmp(func, "loop_accepts")) {
            out = w27_gchat_p4_loop_accepts(in);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
