/*
 * session_events.c — C port of upstream Hermes gateway/session_events.py.
 *
 * PoP: w8_sessioneven_e26544_note_progress @ gateway/session_events.py:SessionEvents.note_progress
 * PoP: w8_sessioneven_e26544_progress_for @ gateway/session_events.py:SessionEvents.progress_for
 *
 * Faithful port of the authority-local progress bookkeeping. time.time() is
 * injected as `now` so the frame timestamp is deterministic; everything else
 * (generation fence, tool.start/tool.complete, str() coercion, newest-open-tool
 * by insertion order, dict.get fallbacks) matches the Python source.
 */
#include "noc_w8_sessioneven_e26544.h"
#include "hermes_json.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Recursive JSON scalar/container equality, mirroring Python == for the
 * value shapes used here (numbers compare numerically, 1 == 1.0). */
static int jeq(const struct json_t *a, const struct json_t *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) {
        if (a->type == JSON_NUMBER && b->type == JSON_NUMBER)
            return a->num_val == b->num_val;
        return 0;
    }
    switch (a->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return a->str_val && b->str_val && !strcmp(a->str_val, b->str_val);
        case JSON_ARRAY: {
            if (json_len(a) != json_len(b)) return 0;
            for (size_t i = 0; i < json_len(a); i++)
                if (!jeq(json_get(a, i), json_get(b, i))) return 0;
            return 1;
        }
        case JSON_OBJECT: {
            if (json_object_size(a) != json_object_size(b)) return 0;
            for (size_t i = 0; i < json_object_size(a); i++) {
                const char *k = json_object_get_key_at(a, i);
                if (!k || !jeq(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
            }
            return 1;
        }
    }
    return 0;
}

/* Python truthiness for the JSON value shapes used in note_progress. */
static int truthy(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return json_len(v) != 0 ? 1 : 0;
        default:          return 0;
    }
}

/* str(value) for the JSON value shapes used here. Fixtures keep non-string
 * name/tool_id values as whole numbers or bools so this is exact. */
static void py_str(const struct json_t *v, char *buf, size_t n) {
    buf[0] = '\0';
    if (!v) return;
    switch (v->type) {
        case JSON_STRING:
            if (v->str_val) snprintf(buf, n, "%s", v->str_val);
            break;
        case JSON_NUMBER:
            if (v->num_val == floor(v->num_val)) snprintf(buf, n, "%.0f", v->num_val);
            else snprintf(buf, n, "%g", v->num_val);
            break;
        case JSON_BOOL:
            snprintf(buf, n, "%s", v->bool_val ? "True" : "False");
            break;
        default:
            break;
    }
}

struct json_t *w8_sessioneven_e26544_note_progress(const struct json_t *state,
                                                    const struct json_t *generation,
                                                    const char *event_type,
                                                    const struct json_t *payload,
                                                    double now) {
    struct json_t *progress = (state && state->type == JSON_OBJECT) ? json_copy(state) : NULL;
    if (!progress || !jeq(json_obj_get(progress, "generation"), generation)) {
        json_free(progress);
        progress = json_object();
        json_set(progress, "generation", json_copy(generation));
        json_set(progress, "tools", json_object());
    }
    struct json_t *tools = json_obj_get(progress, "tools");
    if (!tools || tools->type != JSON_OBJECT) {
        tools = json_object();
        json_set(progress, "tools", tools);
    }
    char tool_id[512];
    py_str(truthy(json_obj_get(payload, "tool_id")) ? json_obj_get(payload, "tool_id") : NULL,
           tool_id, sizeof(tool_id));

    if (event_type && !strcmp(event_type, "tool.start")) {
        char name[512];
        const struct json_t *nv = json_obj_get(payload, "name");
        py_str(truthy(nv) ? nv : NULL, name, sizeof(name));
        if (!name[0]) snprintf(name, sizeof(name), "tool");
        json_set(tools, tool_id, json_string(name)); /* json_set keeps insertion slot */
    } else if (event_type && !strcmp(event_type, "tool.complete")) {
        json_obj_del(tools, tool_id); /* dict.pop(id, None) */
    }
    json_set(progress, "at", json_number(now));
    return progress;
}

struct json_t *w8_sessioneven_e26544_progress_for(const struct json_t *state,
                                                  const struct json_t *generation) {
    struct json_t *out = json_object();
    if (!state || state->type != JSON_OBJECT || !jeq(json_obj_get(state, "generation"), generation))
        return out; /* {} */
    struct json_t *at = json_obj_get(state, "at");
    json_set(out, "progress_at", json_copy(at ? at : json_null()));
    struct json_t *tools = json_obj_get(state, "tools");
    size_t n = (tools && tools->type == JSON_OBJECT) ? json_object_size(tools) : 0;
    if (n) {
        struct json_t *last = json_object_get_at(tools, n - 1); /* newest open tool (insertion order) */
        json_set(out, "tool", json_copy(last ? last : json_null()));
    }
    return out;
}
