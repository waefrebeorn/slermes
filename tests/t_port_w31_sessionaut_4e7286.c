/* JSON-lines harness for the w31_sessionaut_4e7286 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back unchanged. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_sessionaut_4e7286.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "note"))
        return w31_sessionaut_4e7286_note(json_obj_get(in, "prompt"));
    if (!strcmp(func, "config"))
        return w31_sessionaut_4e7286_config(json_obj_get(in, "cfg"));
    if (!strcmp(func, "busy_mode"))
        return w31_sessionaut_4e7286_busy_mode(json_obj_get(in, "queued"),
                                               json_obj_get(in, "configured"),
                                               json_obj_get(in, "compression"));
    if (!strcmp(func, "inflight_original"))
        return w31_sessionaut_4e7286_inflight_original(json_obj_get(in, "session"));
    if (!strcmp(func, "sanitize_entry"))
        return w31_sessionaut_4e7286_sanitize_entry(json_obj_get(in, "entry"),
                                                    json_obj_get(in, "original"));
    if (!strcmp(func, "set_queue"))
        return w31_sessionaut_4e7286_set_queue(json_obj_get(in, "session"),
                                               json_obj_get(in, "entries"));
    if (!strcmp(func, "enqueue"))
        return w31_sessionaut_4e7286_enqueue(json_obj_get(in, "session"),
                                             json_obj_get(in, "text"),
                                             json_obj_get(in, "transport"),
                                             json_obj_get(in, "image_paths"),
                                             json_obj_get(in, "turn_author"));
    if (!strcmp(func, "inflight_snapshot"))
        return w31_sessionaut_4e7286_inflight_snapshot(json_obj_get(in, "session"));
    if (!strcmp(func, "queued_snapshot"))
        return w31_sessionaut_4e7286_queued_snapshot(json_obj_get(in, "session"));
    if (!strcmp(func, "maybe_schedule"))
        return w31_sessionaut_4e7286_maybe_schedule(json_obj_get(in, "session"),
                                                    json_obj_get(in, "marker"),
                                                    json_obj_get(in, "cfg"),
                                                    json_obj_get(in, "writer_state"),
                                                    json_obj_get(in, "now"),
                                                    json_obj_get(in, "pid"));
    return json_string("UNKNOWN_FUNC");
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
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
        if (json_object_size(in)) {
            size_t i;
            for (i = 0; i < json_object_size(in); i++) {
                const char *k = json_object_get_key_at(in, i);
                if (!k || !strcmp(k, "func")) continue;
                fputs(",", stdout);
                emit_key(k, json_copy(json_object_get_at(in, i)));
            }
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
