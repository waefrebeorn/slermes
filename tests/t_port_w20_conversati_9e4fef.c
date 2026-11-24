/* JSON-lines harness for the w20_conversati_9e4fef gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_conversati_9e4fef.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "auto_mode")) {
            char *s = w20_conversati_9e4fef_auto_mode(json_get_str(in, "raw", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "tokens_label")) {
            char *s = w20_conversati_9e4fef_tokens_label(json_obj_get(in, "approx_tokens"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "cooldown_remaining")) {
            out = w20_conversati_9e4fef_cooldown_remaining(json_obj_get(in, "agent"));
        } else if (!strcmp(func, "skip_decision")) {
            out = w20_conversati_9e4fef_skip_decision(in);
        } else if (!strcmp(func, "failed")) {
            out = json_bool(w20_conversati_9e4fef_failed(json_obj_get(in, "result")) ? 1 : 0);
        } else if (!strcmp(func, "failure_message")) {
            char *s = w20_conversati_9e4fef_failure_message(json_obj_get(in, "result"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "failure_error")) {
            char *s = w20_conversati_9e4fef_failure_error(json_obj_get(in, "result"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "status_text")) {
            out = json_string(w20_conversati_9e4fef_status_text(json_get_bool(in, "failed", 0) ? 1 : 0));
        } else if (!strcmp(func, "plan")) {
            out = w20_conversati_9e4fef_plan(in);
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
