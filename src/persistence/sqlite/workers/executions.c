/*
 * executions.c - real C11 port of upstream Hermes
 *   persistence/sqlite/workers/executions.py pure helpers and wire-payload builders.
 * The repository itself is SQLite plumbing; what is portable is the key-set fence, the
 * message/record coercion and the exact payload each verb hands its receipt transaction.
 * PoP: w19_executions_57e1de_key_set @ persistence/sqlite/workers/executions.py:_key_set
 * PoP: w19_executions_57e1de_messages @ persistence/sqlite/workers/executions.py:_messages
 * PoP: w19_executions_57e1de_record @ persistence/sqlite/workers/executions.py:_record
 * PoP: w19_executions_57e1de_payload_usage_main @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.record_usage
 * PoP: w19_executions_57e1de_payload_usage_auxiliary @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.record_auxiliary_usage
 * PoP: w19_executions_57e1de_payload_create @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.create_session
 * PoP: w19_executions_57e1de_payload_append @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.append_transcript
 * PoP: w19_executions_57e1de_payload_user_content @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.set_user_message_content
 * PoP: w19_executions_57e1de_payload_restore_cooldown @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.restore_compression_cooldown
 * PoP: w19_executions_57e1de_payload_lease @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.acquire_turn_lease
 * PoP: w19_executions_57e1de_payload_boundary @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.compression_boundary
 * PoP: w19_executions_57e1de_payload_activity @ persistence/sqlite/workers/executions.py:SQLiteWorkerExecutionRepository.touch_session_activity
 */
#include "hermes_json.h"
#include "noc_w19_executions_57e1de.h"

static json_t *jget(json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static json_t *cp(json_t *v) { return v ? json_copy(v) : json_null(); }

static json_t *ok_env(const char *operation, json_t *payload) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "operation", json_string(operation));
    json_set(o, "payload", payload);
    return o;
}

static json_t *err_env(const char *error) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string(error));
    return o;
}

/* Membership of one key token in `allowed` (array of strings). */
static int str_in(json_t *list, const char *s) {
    if (!list || list->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(list); i++) {
        json_t *e = json_get(list, i);
        if (e && e->type == JSON_STRING && !strcmp(e->str_val, s)) return 1;
    }
    return 0;
}

/* set(keys) as seen by the live helper: an object contributes its keys, a list its items. */
static size_t keys_len(json_t *keys) {
    if (!keys) return 0;
    if (keys->type == JSON_OBJECT) return json_object_size(keys);
    if (keys->type == JSON_ARRAY) return json_len(keys);
    return 0;
}

static const char *keys_at(json_t *keys, size_t i) {
    if (!keys) return NULL;
    if (keys->type == JSON_OBJECT) return json_object_get_key_at(keys, i);
    if (keys->type == JSON_ARRAY) {
        json_t *e = json_get(keys, i);
        return (e && e->type == JSON_STRING) ? e->str_val : NULL;
    }
    return NULL;
}

/* True when a non-string token sits in `keys` (it cannot match a string allowed set). */
static int keys_have_nonstring(json_t *keys) {
    if (!keys || keys->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(keys); i++) {
        json_t *e = json_get(keys, i);
        if (!e || e->type != JSON_STRING) return 1;
    }
    return 0;
}

static int key_set_ok(json_t *keys, json_t *allowed, json_t *required) {
    if (!keys || (keys->type != JSON_OBJECT && keys->type != JSON_ARRAY)) return 0;
    if (keys_have_nonstring(keys)) return 0;
    size_t n = keys_len(keys);
    for (size_t i = 0; i < n; i++) {
        const char *k = keys_at(keys, i);
        if (!k || !str_in(allowed, k)) return 0;
    }
    if (required && required->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(required); i++) {
            json_t *r = json_get(required, i);
            if (!r || r->type != JSON_STRING) continue;
            int found = 0;
            for (size_t j = 0; j < n && !found; j++) {
                const char *k = keys_at(keys, j);
                if (k && !strcmp(k, r->str_val)) found = 1;
            }
            if (!found) return 0;
        }
    }
    return 1;
}

json_t *w19_executions_57e1de_key_set(json_t *keys, json_t *allowed, json_t *required) {
    return json_string(key_set_ok(keys, allowed, required) ? "ok" : "invalid_params");
}

json_t *w19_executions_57e1de_messages(json_t *messages) {
    json_t *out = json_array();
    if (!messages || messages->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(messages); i++) {
        json_t *m = json_get(messages, i);
        /* dict(message) for a mapping; any other item passes through */
        if (m && m->type == JSON_OBJECT) {
            json_t *copy = json_object();
            for (size_t k = 0; k < json_object_size(m); k++)
                json_set(copy, json_object_get_key_at(m, k),
                         cp(json_object_get_at(m, k)));
            json_append(out, copy);
        } else {
            json_append(out, cp(m));
        }
    }
    return out;
}

static int str_one_of(const char *s, const char *const *vals) {
    if (!s) return 0;
    for (int i = 0; vals[i]; i++)
        if (!strcmp(s, vals[i])) return 1;
    return 0;
}

static const char *const WORKER_KINDS[] = { "cron", "child", "compute", "kanban", NULL };
static const char *const WORKER_STATUSES[] = { "registered", "running", "unknown", "terminal", NULL };

json_t *w19_executions_57e1de_record(json_t *public_fields) {
    static const char *const FIELDS[] = {
        "execution_id", "session_id", "kind", "owner_epoch", "generation", "status",
        "last_sequence", NULL
    };
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_t *value = json_object();
    json_set(out, "value", value);
    if (!public_fields || public_fields->type != JSON_OBJECT) {
        json_set(out, "ok", json_bool(false));
        json_set(out, "error", json_string("KeyError"));
        json_obj_del(out, "value");
        return out;
    }
    for (int i = 0; FIELDS[i]; i++) {
        if (!json_has(public_fields, FIELDS[i])) {
            json_set(out, "ok", json_bool(false));
            json_set(out, "error", json_string("KeyError"));
            json_obj_del(out, "value");
            return out;
        }
        json_set(value, FIELDS[i], cp(json_obj_get(public_fields, FIELDS[i])));
    }
    json_t *kind = json_obj_get(value, "kind");
    if (!kind || kind->type != JSON_STRING || !str_one_of(kind->str_val, WORKER_KINDS)) {
        json_set(out, "ok", json_bool(false));
        json_set(out, "error", json_string("ValueError"));
        json_obj_del(out, "value");
        return out;
    }
    json_t *status = json_obj_get(value, "status");
    if (!status || status->type != JSON_STRING || !str_one_of(status->str_val, WORKER_STATUSES)) {
        json_set(out, "ok", json_bool(false));
        json_set(out, "error", json_string("ValueError"));
        json_obj_del(out, "value");
        return out;
    }
    json_t *seq = json_obj_get(value, "last_sequence");
    if (!seq || seq->type != JSON_NUMBER || seq->num_val < 0.0) {
        json_set(out, "ok", json_bool(false));
        json_set(out, "error", json_string("ValueError"));
        json_obj_del(out, "value");
        return out;
    }
    return out;
}

json_t *w19_executions_57e1de_payload_usage_main(json_t *usage, json_t *allowed) {
    if (!key_set_ok(usage, allowed, NULL)) return err_env("invalid_params");
    json_t *payload = json_object();
    if (usage) {
        for (size_t i = 0; i < json_object_size(usage); i++)
            json_set(payload, json_object_get_key_at(usage, i), cp(json_object_get_at(usage, i)));
    }
    return ok_env("usage.main", payload);
}

json_t *w19_executions_57e1de_payload_usage_auxiliary(json_t *usage, const char *task,
                                                      json_t *allowed) {
    if (!key_set_ok(usage, allowed, NULL)) return err_env("invalid_params");
    json_t *payload = json_object();
    if (usage) {
        for (size_t i = 0; i < json_object_size(usage); i++)
            json_set(payload, json_object_get_key_at(usage, i), cp(json_object_get_at(usage, i)));
    }
    json_set(payload, "task", json_string(task ? task : ""));
    return ok_env("usage.auxiliary", payload);
}

json_t *w19_executions_57e1de_payload_create(json_t *fields, json_t *allowed) {
    json_t *required = json_array();
    json_append(required, json_string("source"));
    int ok = key_set_ok(fields, allowed, required);
    json_free(required);
    if (!ok) return err_env("invalid_params");
    json_t *payload = json_object();
    for (size_t i = 0; i < json_object_size(fields); i++)
        json_set(payload, json_object_get_key_at(fields, i), cp(json_object_get_at(fields, i)));
    return ok_env("session.create", payload);
}

json_t *w19_executions_57e1de_payload_append(json_t *messages, json_t *options, json_t *allowed) {
    if (!key_set_ok(options, allowed, NULL)) return err_env("invalid_params");
    json_t *payload = json_object();
    json_set(payload, "messages", w19_executions_57e1de_messages(messages));
    if (options) {
        for (size_t i = 0; i < json_object_size(options); i++)
            json_set(payload, json_object_get_key_at(options, i), cp(json_object_get_at(options, i)));
    }
    return ok_env("transcript.append", payload);
}

json_t *w19_executions_57e1de_payload_user_content(json_t *row_id, json_t *content,
                                                   json_t *display_metadata,
                                                   int has_display_metadata) {
    json_t *payload = json_object();
    json_set(payload, "row_id", cp(row_id));
    json_set(payload, "content", cp(content));
    if (has_display_metadata && display_metadata) {
        json_set(payload, "display_metadata", cp(display_metadata));
    }
    return ok_env("session.user_content", payload);
}

json_t *w19_executions_57e1de_payload_restore_cooldown(json_t *snapshot) {
    json_t *inner = json_object();
    json_set(inner, "session_exists", cp(jget(snapshot, "session_exists")));
    json_set(inner, "cooldown_until", cp(jget(snapshot, "cooldown_until")));
    json_set(inner, "error", cp(jget(snapshot, "error")));
    json_t *payload = json_object();
    json_set(payload, "snapshot", inner);
    return ok_env("compression.cooldown.restore", payload);
}

json_t *w19_executions_57e1de_payload_lease(const char *holder, json_t *ttl_seconds) {
    json_t *payload = json_object();
    json_set(payload, "holder", json_string(holder ? holder : ""));
    json_set(payload, "ttl_seconds", cp(ttl_seconds));
    return ok_env("turn.acquire", payload);
}

json_t *w19_executions_57e1de_payload_boundary(json_t *peer) {
    json_t *payload = json_object();
    json_set(payload, "session_key", cp(jget(peer, "session_key")));
    json_set(payload, "source", cp(jget(peer, "source")));
    return ok_env("compression.boundary", payload);
}

json_t *w19_executions_57e1de_payload_activity(json_t *activity) {
    json_t *payload = json_object();
    json_set(payload, "ts", cp(jget(activity, "ts")));
    json_set(payload, "description", cp(jget(activity, "description")));
    json_set(payload, "provenance", cp(jget(activity, "provenance")));
    return ok_env("session.activity", payload);
}
