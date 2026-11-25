/* JSON-lines harness for the w21_emitter_1f24b4 gate (agent/monitoring/emitter).
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * A line is a scenario: {"func":"run","enabled":true,"capacity":2,"ops":[...]}
 * and "out" is the per-op record array (queue depths, drained batches, ops
 * counters, stats) the oracle replays against the live MonitoringEmitter.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_emitter_1f24b4.h"
#include "agent/monitoring/emitter.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static double stat_of(w21_emitter_1f24b4 *st, const char *key) {
    json_t *s = w21_emitter_1f24b4_stats(st);
    double v = json_get_num(s, key, 0.0);
    json_free(s);
    return v;
}

static json_t *run_scenario(json_t *in) {
    json_t *ops = json_obj_get(in, "ops");
    json_t *records = json_array();
    json_t *enabled_v = json_obj_get(in, "enabled");
    json_t *cap_v = json_obj_get(in, "capacity");
    int enabled = 1;
    size_t capacity = 10000;
    w21_emitter_1f24b4 *st;
    size_t i;

    if (enabled_v && enabled_v->type == JSON_BOOL) enabled = enabled_v->bool_val ? 1 : 0;
    if (cap_v && cap_v->type == JSON_NUMBER) capacity = (size_t)cap_v->num_val;
    st = w21_emitter_1f24b4_new(enabled, capacity);
    if (!st) return json_string("OOM");
    if (!ops || ops->type != JSON_ARRAY) {
        w21_emitter_1f24b4_free(st);
        return records;
    }
    for (i = 0; i < ops->c.count; i++) {
        json_t *op = ops->c.items[i];
        const char *kind;
        json_t *rec = json_object();
        if (!op || op->type != JSON_OBJECT) continue;
        kind = json_get_str(op, "op", "");
        if (!strcmp(kind, "emit")) {
            double now = json_get_num(op, "now", 0.0);
            w21_emitter_1f24b4_emit(st, json_obj_get(op, "event"), (long long)now);
            json_set(rec, "op", json_string("emit"));
            json_set(rec, "queued", json_number(stat_of(st, "queued")));
            json_set(rec, "dropped", json_number(stat_of(st, "dropped")));
        } else if (!strcmp(kind, "drain")) {
            json_t *batch = w21_emitter_1f24b4_dispatch(st);
            json_set(rec, "op", json_string("drain"));
            json_set(rec, "batch", batch);
            json_set(rec, "dispatched", json_number(stat_of(st, "dispatched")));
        } else if (!strcmp(kind, "subscribe")) {
            w21_emitter_1f24b4_subscribe(st, json_get_str(op, "id", ""),
                                         json_get_bool(op, "failing", 0));
            json_set(rec, "op", json_string("subscribe"));
            json_set(rec, "subscribers", json_number(stat_of(st, "subscribers")));
            json_set(rec, "enabled", json_bool(w21_emitter_1f24b4_enabled(st)));
        } else if (!strcmp(kind, "unsubscribe")) {
            w21_emitter_1f24b4_unsubscribe(st, json_get_str(op, "id", ""));
            json_set(rec, "op", json_string("unsubscribe"));
            json_set(rec, "subscribers", json_number(stat_of(st, "subscribers")));
            json_set(rec, "enabled", json_bool(w21_emitter_1f24b4_enabled(st)));
        } else if (!strcmp(kind, "stats")) {
            json_set(rec, "op", json_string("stats"));
            json_set(rec, "stats", w21_emitter_1f24b4_stats(st));
        } else {
            json_set(rec, "op", json_string("unknown"));
        }
        json_append(records, rec);
    }
    w21_emitter_1f24b4_free(st);
    return records;
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "run")) {
            out = run_scenario(in);
        } else if (!strcmp(func, "constants")) {
            out = json_object();
            json_set(out, "queue_cap", json_number((double)w21_emitter_1f24b4_queue_cap()));
            json_set(out, "drain_batch", json_number((double)w21_emitter_1f24b4_drain_batch()));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
