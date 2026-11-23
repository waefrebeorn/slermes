/*
 * worker.c — C port of upstream Hermes pm/worker.py (gate w5_worker_9c3898).
 *
 * PoP: w5_worker_9c3898_read_controls @ pm/worker.py:_read_controls
 *
 * _read_controls reads newline-delimited JSON control messages from a private
 * fd: the first message fixes the request id, every later message must match,
 * a truthy "cancel" sets the pause event, and messages whose type is not
 * "cancel" are queued. OSError/ValueError/KeyError are delivered to the queue;
 * the finally block always closes the fd, sets pause and pushes None. Here the
 * messages are supplied already decoded and the result is pure JSON.
 */
#include "noc_w5_worker_9c3898.h"

#include <stdlib.h>
#include <string.h>

static int json_truthy(json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && *v->str_val;
        case JSON_ARRAY: return (int)json_len(v) > 0;
        case JSON_OBJECT: return (int)json_object_size(v) > 0;
    }
    return 0;
}

/* json_dumps gives a canonical comparable spelling for an id of any type. */
static int same_id(json_t *a, json_t *b) {
    if (!a || !b) return 0;
    char *sa = json_dumps(a, 0), *sb = json_dumps(b, 0);
    int eq = sa && sb && !strcmp(sa, sb);
    free(sa); free(sb);
    return eq;
}

json_t *w5_worker_9c3898_read_controls(json_t *messages) {
    json_t *queued = json_array();
    json_t *request_id = NULL;
    const char *error = NULL;
    int paused = 0;

    size_t n = (messages && messages->type == JSON_ARRAY) ? json_len(messages) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *msg = json_get(messages, i);
        if (!msg || msg->type != JSON_OBJECT) {
            error = "TypeError: 'list' object is not subscriptable";
            break;
        }
        json_t *idv = json_obj_get(msg, "id");
        if (!idv) { error = "KeyError: 'id'"; break; }
        if (!request_id) request_id = idv;
        else if (!same_id(idv, request_id)) {
            error = "ValueError: unexpected PM control request id";
            break;
        }
        if (json_truthy(json_obj_get(msg, "cancel"))) paused = 1;
        json_t *typev = json_obj_get(msg, "type");
        const char *type = (typev && typev->type == JSON_STRING) ? typev->str_val : "";
        if (strcmp(type, "cancel") != 0) json_append(queued, json_copy(msg));
    }

    /* finally: os.close(fd); pause.set(); messages.put(None) */
    paused = 1;

    json_t *out = json_object();
    json_set(out, "messages", queued);
    json_set(out, "paused", json_bool(paused));
    json_set(out, "error", error ? json_string(error) : json_null());
    return out;
}
