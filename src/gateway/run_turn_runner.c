/*
 * run_turn_runner.c — C home for upstream Hermes gateway/run_turn_runner.py.
 *
 * PoP: w9_runturnrun_42e645_observed_persist_message @ gateway/run_turn_runner.py:_observed_turn_persist_message
 * PoP: w9_runturnrun_42e645_merge_overrides @ gateway/run_turn_runner.py:TurnRunner._merge_turn_request_overrides
 *
 * Two pure helpers from the per-turn callback runner, ported statement-for-
 * statement from the live Python (which cannot be imported in this build
 * environment because of its heavy optional dependencies).
 */
#include "noc_w9_runturnrun_42e645.h"

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python value equality (objects compared key-wise, not by wire order). */
static int json_equal(const json_t *a, const json_t *b) {
    if (a == b) return 1;
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; ++i)
                if (!json_equal(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; ++i) {
                const char *k = a->c.keys[i];
                json_t *bv = json_obj_get(b, k);
                if (!bv || !json_equal(a->c.items[i], bv)) return 0;
            }
            return 1;
    }
    return 0;
}

/* str(v) with no `or ""` guard: JSON null -> Python "None". */
static const char *py_str_full(const json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) return "None";
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    if (v->type == JSON_BOOL)   return v->bool_val ? "True" : "False";
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, n, "%lld", (long long)v->num_val);
        else
            snprintf(buf, n, "%g", v->num_val);
        return buf;
    }
    return "";
}

struct json_t *w9_runturnrun_42e645_observed_persist_message(const struct json_t *run_message,
                                                             const struct json_t *override,
                                                             const struct json_t *live_text) {
    int override_none = !override || override->type == JSON_NULL;
    /* if not isinstance(run_message, list): return override if override is not None else live_text */
    if (!run_message || run_message->type != JSON_ARRAY)
        return json_copy(override_none ? live_text : override);

    json_t *first = run_message->c.count ? run_message->c.items[0] : NULL;
    int is_override_str = (!override_none && override->type == JSON_STRING);
    int is_first_dict   = (first && first->type == JSON_OBJECT);
    int is_live_str     = (live_text && live_text->type == JSON_STRING);
    int matched = 0;
    const char *first_text = "";
    char nbuf[64];

    if (is_override_str && is_first_dict && is_live_str &&
        !strcmp(json_get_str(first, "type", ""), "text")) {
        json_t *tv = json_obj_get(first, "text");
        char tbuf[64];
        first_text = tv ? py_str_full(tv, tbuf, sizeof tbuf) : "";
        const char *live = live_text->str_val ? live_text->str_val : "";
        matched = strncmp(first_text, live, strlen(live)) == 0;
        (void)nbuf;
    }

    if (!matched)
        return json_copy(run_message);

    /* [{**first, "text": override + first["text"][len(live_text):]}, *run_message[1:]] */
    const char *live = live_text->str_val ? live_text->str_val : "";
    size_t cut = strlen(live);
    if (cut > strlen(first_text)) cut = strlen(first_text);
    const char *ovr = override->str_val ? override->str_val : "";
    size_t nl = strlen(ovr) + strlen(first_text + cut) + 1;
    char *joined = (char *)malloc(nl);
    if (joined) {
        snprintf(joined, nl, "%s%s", ovr, first_text + cut);
    }
    json_t *out = json_array();
    json_t *newfirst = json_copy(first);
    json_set(newfirst, "text", json_string(joined ? joined : ""));
    json_append(out, newfirst);
    for (size_t i = 1; i < run_message->c.count; ++i)
        json_append(out, json_copy(run_message->c.items[i]));
    free(joined);
    return out;
}

struct json_t *w9_runturnrun_42e645_merge_overrides(const struct json_t *agent_overrides,
                                                    const struct json_t *prev_overrides,
                                                    const struct json_t *turn_overrides) {
    json_t *overrides = json_object();
    if (agent_overrides && agent_overrides->type == JSON_OBJECT && agent_overrides->c.count)
        for (size_t i = 0; i < agent_overrides->c.count; ++i)
            json_set(overrides, agent_overrides->c.keys[i], json_copy(agent_overrides->c.items[i]));

    /* for key, value in prev.items(): if overrides.get(key) == value: pop */
    if (prev_overrides && prev_overrides->type == JSON_OBJECT)
        for (size_t i = 0; i < prev_overrides->c.count; ++i) {
            const char *k = prev_overrides->c.keys[i];
            json_t *cur = json_obj_get(overrides, k);
            if (json_equal(cur, prev_overrides->c.items[i]))
                json_obj_del(overrides, k);
        }

    json_t *turn = json_object();
    if (turn_overrides && turn_overrides->type == JSON_OBJECT && turn_overrides->c.count)
        for (size_t i = 0; i < turn_overrides->c.count; ++i)
            json_set(turn, turn_overrides->c.keys[i], json_copy(turn_overrides->c.items[i]));

    for (size_t i = 0; i < turn->c.count; ++i)
        json_set(overrides, turn->c.keys[i], json_copy(turn->c.items[i]));

    json_t *out = json_object();
    json_set(out, "overrides", overrides);
    json_set(out, "prev", turn);
    return out;
}
