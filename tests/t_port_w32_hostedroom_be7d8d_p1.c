/* JSON-lines harness for the w32_hostedroom_be7d8d_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_hostedroom_be7d8d_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
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
        if (!strcmp(func, "budget")) {
            out = w32_hostedroom_be7d8d_p1_budget(json_obj_get(in, "max_active_events"),
                                                  json_get_str(in, "value_type", "int"));
        } else if (!strcmp(func, "capacity")) {
            out = w32_hostedroom_be7d8d_p1_capacity(json_obj_get(in, "cached"), json_obj_get(in, "stored"),
                                                    json_get_num(in, "budget", 0.0));
        } else if (!strcmp(func, "transcript_bound")) {
            out = w32_hostedroom_be7d8d_p1_transcript_bound(json_get_num(in, "max_active_events", 0.0));
        } else if (!strcmp(func, "snapshot_thread")) {
            out = w32_hostedroom_be7d8d_p1_snapshot_thread(in);
        } else if (!strcmp(func, "replay_state")) {
            out = w32_hostedroom_be7d8d_p1_replay_state(in);
        } else if (!strcmp(func, "sync_step")) {
            out = w32_hostedroom_be7d8d_p1_sync_step(in);
        } else if (!strcmp(func, "settled_sources")) {
            out = w32_hostedroom_be7d8d_p1_settled_sources(json_obj_get(in, "events"));
        } else if (!strcmp(func, "task_event_ids")) {
            out = w32_hostedroom_be7d8d_p1_task_event_ids(json_obj_get(in, "task_id"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        (void)opt_str;
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
