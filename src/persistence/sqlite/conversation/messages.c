/*
 * messages.c — C home for upstream Hermes persistence/sqlite/conversation/messages.py.
 *
 * Upstream file: persistence/sqlite/conversation/messages.py (the SQLite message
 * repository: the transcript verbs over the conversation store functions).
 *
 * STATUS: behavior port — the pure logic of the module, split across three
 * differential gates that all share this one port file:
 *
 *   w30_messages_29cf8f_p1  wire shaping    (_REWIND_REFUSALS .. _sidecar)
 *   w30_messages_29cf8f_p2  row stamps      (_message_record .. cooldowns)
 *   w30_messages_29cf8f_p3  SQL text        (statement builders, read envelopes)
 *
 * Every SQLite seam stays in the caller and is NOT ported here: the
 * StoreHolder (write pool, read pool, lock waits, WAL accounting), the
 * sqlite3 connections and transactions, the store's own writers/readers
 * (append_message, replace_messages, get_messages, ...), the session rules
 * (message_uid_or_none, _stamp_tool_call_uids), error classification
 * (classify_persistence_error) and the typed value objects. What is ported is
 * exactly what those values decide: SQL fragment/statement text, row and
 * envelope shaping, ordering/merge rules, title/preview derivation,
 * coercion and predicates.
 *
 * Exported pure functions: 33 (8 + 11 + 14), each with a PoP line naming the
 * upstream symbol it ports. Pure units left unported, by design:
 *   _conversation        delegates to ConversationMessage.from_openai_message
 *                        (a typed value object defined in another module);
 *   _refusals            a context manager over the store's error
 *                        classification (classify_persistence_error and the
 *                        contract error types); its pure reason table
 *                        (_REWIND_REFUSALS) is ported in gate p1;
 *   the holder/connection surface itself (StoreHolder pools, lock waits,
 *                        transactions, the store's writers and readers).
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w30_messages_29cf8f_p1.h"
#include "noc_w30_messages_29cf8f_p2.h"
#include "noc_w30_messages_29cf8f_p3.h"

/* Store key constants the shaping writes (conversation/_store/rules.py and
 * session_rules/turns.py values, quoted here as the module imports them). */
#define MSG_DB_ROW_SNAPSHOT "_db_row_snapshot"
#define MSG_RESTORED_FROM_STORE "_restored_from_store"
#define MSG_DB_PERSISTED_MARKER "_db_persisted"
#define MSG_MERGED_DURABLE_ROWS "_merged_durable_rows"
#define MSG_UNNAMED_DURABLE_ROWS "_unnamed_durable_rows"
#define MSG_RETIRED_DURABLE_ROWS "_retired_durable_rows"
#define MSG_RETIRED_ROW "_retired_row"
#define MSG_OWN_ROW "_own_row"

/* ── shared value helpers ─────────────────────────────────────────────── */

/* Python truthiness of a JSON value: absent/None/False/0/""/[]/{} are falsy. */
static int msg_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* Python ``x is not None``: present and not JSON null. */
static int msg_is_not_none(const json_t *v) { return v != NULL && v->type != JSON_NULL; }

static json_t *msg_copy_or_null(const json_t *v) { return v ? json_copy(v) : json_null(); }

static void msg_set_copy(json_t *obj, const char *key, const json_t *v) {
    if (v) json_set(obj, key, json_copy(v));
}

/* ``key: value`` for a value that is not None. */
static void msg_set_not_none(json_t *obj, const char *key, const json_t *v) {
    if (msg_is_not_none(v)) json_set(obj, key, json_copy(v));
}

/* ``key: True`` for a truthy flag. */
static void msg_set_true_if(json_t *obj, const char *key, const json_t *v) {
    if (msg_truthy(v)) json_set(obj, key, json_bool(true));
}

/* int(x): truncation toward zero, 0 for a missing/non-number value. */
static long long msg_int_value(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? (long long)v->num_val : 0;
}

static json_t *msg_number_of(const json_t *v) { return json_number((double)msg_int_value(v)); }

/* [int(i) for i in values]. */
static json_t *msg_int_list(const json_t *values) {
    json_t *out = json_array();
    size_t n = json_len(values);
    for (size_t i = 0; i < n; i++) json_append(out, msg_number_of(json_get(values, i)));
    return out;
}

/* ── gate p1: wire shaping ────────────────────────────────────────────── */

/* PoP: w30_messages_29cf8f_p1_rewind_refusal_reason @ persistence/sqlite/conversation/messages.py:_REWIND_REFUSALS */
json_t *w30_messages_29cf8f_p1_rewind_refusal_reason(const json_t *in) {
    static const char *const TEXTS[] = {
        "session history changed before the rewind could be persisted",
        "active transcript changed before the rewind could be persisted",
        "rewind target has no durable row identity",
        "rewind target changed before it could be persisted",
        "rewind did not retain its compaction handoff",
    };
    static const char *const REASONS[] = {
        "history_changed", "history_changed", "target_unidentified", "target_changed", "handoff_lost",
    };
    const char *text = json_get_str(in, "text", NULL);
    if (text) {
        for (size_t i = 0; i < sizeof(TEXTS) / sizeof(TEXTS[0]); i++)
            if (!strcmp(text, TEXTS[i])) return json_string(REASONS[i]);
    }
    return json_null();
}

/* PoP: w30_messages_29cf8f_p1_visibility @ persistence/sqlite/conversation/messages.py:_visibility */
json_t *w30_messages_29cf8f_p1_visibility(const json_t *in) {
    if (msg_truthy(json_obj_get(in, "active"))) return json_string("active");
    if (msg_truthy(json_obj_get(in, "compacted"))) return json_string("compaction_archived");
    return json_string("rewound");
}

/* _MESSAGE_OPTIONALS: NewMessage fields that travel under their own name. */
static const char *const MSG_MESSAGE_OPTIONALS[] = {
    "tool_name", "tool_call_id", "display_kind", "api_content", "platform_message_id", "effect_disposition",
    "finish_reason", "reasoning", "reasoning_content", "reasoning_details", "codex_reasoning_items",
    "codex_message_items", "token_count", "message_uid",
};

/* _WIRE_FIELDS: the stored columns a TranscriptRow carries. */
static const char *const MSG_WIRE_FIELDS[] = {
    "role", "content", "timestamp", "tool_call_id", "tool_calls", "tool_name", "effect_disposition", "token_count",
    "finish_reason", "reasoning", "reasoning_content", "reasoning_details", "codex_reasoning_items",
    "codex_message_items", "platform_message_id", "api_content", "display_kind", "display_metadata", "message_uid",
    "absorbed_message_uids", "tool_call_uids", "tool_call_uid",
};

/* _SIDECARS: the three columns stored as JSON text and decoded on read. */
static const char *const MSG_SIDECARS[] = {"reasoning_details", "codex_reasoning_items", "codex_message_items"};

#define MSG_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* The message dict one NewMessage becomes for the legacy store's writers. */
static json_t *msg_message_dict(const json_t *message) {
    json_t *out = json_object();
    json_t *content;
    json_set(out, "role", json_string(json_get_str(message, "role", "")));
    content = json_obj_get(message, "content");
    json_set(out, "content", content ? json_copy(content) : json_null());
    msg_set_not_none(out, "timestamp", json_obj_get(message, "timestamp"));
    if (msg_truthy(json_obj_get(message, "tool_calls"))) msg_set_copy(out, "tool_calls", json_obj_get(message, "tool_calls"));
    for (size_t i = 0; i < MSG_ARRAY_LEN(MSG_MESSAGE_OPTIONALS); i++) {
        const char *name = MSG_MESSAGE_OPTIONALS[i];
        msg_set_not_none(out, name, json_obj_get(message, name));
    }
    msg_set_not_none(out, "display_metadata", json_obj_get(message, "display_metadata"));
    msg_set_true_if(out, "_compressed_summary", json_obj_get(message, "compressed_summary"));
    msg_set_true_if(out, "observed", json_obj_get(message, "observed"));
    if (msg_is_not_none(json_obj_get(message, "repair_target")))
        json_set(out, "_row_id", msg_number_of(json_obj_get(message, "repair_target")));
    msg_set_not_none(out, MSG_DB_ROW_SNAPSHOT, json_obj_get(message, "db_row_snapshot"));
    if (msg_truthy(json_obj_get(message, "absorbed_message_uids")))
        msg_set_copy(out, "_absorbed_message_uids", json_obj_get(message, "absorbed_message_uids"));
    msg_set_not_none(out, "_tool_call_uids", json_obj_get(message, "tool_call_uids"));
    msg_set_not_none(out, "_tool_call_uid", json_obj_get(message, "tool_call_uid"));
    if (msg_truthy(json_obj_get(message, "data_use"))) msg_set_copy(out, "data_use", json_obj_get(message, "data_use"));
    if (msg_truthy(json_obj_get(message, "merged_durable_rows")))
        msg_set_copy(out, MSG_MERGED_DURABLE_ROWS, json_obj_get(message, "merged_durable_rows"));
    if (msg_truthy(json_obj_get(message, "unnamed_durable_rows")))
        msg_set_copy(out, MSG_UNNAMED_DURABLE_ROWS, json_obj_get(message, "unnamed_durable_rows"));
    if (msg_truthy(json_obj_get(message, "absorbed_row_ids")))
        json_set(out, "_absorbed_row_ids", msg_int_list(json_obj_get(message, "absorbed_row_ids")));
    msg_set_true_if(out, MSG_RESTORED_FROM_STORE, json_obj_get(message, "restored_from_store"));
    return out;
}

/* The held message dict the legacy store compares; retired rows recurse. */
static json_t *msg_held_dict(const json_t *held) {
    json_t *out = json_object();
    json_t *content;
    json_set(out, "role", json_string(json_get_str(held, "role", "")));
    content = json_obj_get(held, "content");
    json_set(out, "content", content ? json_copy(content) : json_null());
    msg_set_not_none(out, "tool_call_id", json_obj_get(held, "tool_call_id"));
    if (msg_truthy(json_obj_get(held, "tool_calls"))) msg_set_copy(out, "tool_calls", json_obj_get(held, "tool_calls"));
    msg_set_not_none(out, "timestamp", json_obj_get(held, "timestamp"));
    if (msg_is_not_none(json_obj_get(held, "row_id")))
        json_set(out, "_row_id", msg_number_of(json_obj_get(held, "row_id")));
    msg_set_true_if(out, MSG_DB_PERSISTED_MARKER, json_obj_get(held, "persisted"));
    msg_set_not_none(out, MSG_DB_ROW_SNAPSHOT, json_obj_get(held, "db_row_snapshot"));
    msg_set_not_none(out, "message_uid", json_obj_get(held, "message_uid"));
    if (msg_truthy(json_obj_get(held, "absorbed_row_ids")))
        json_set(out, "_absorbed_row_ids", msg_int_list(json_obj_get(held, "absorbed_row_ids")));
    if (msg_truthy(json_obj_get(held, "merged_durable_rows")))
        msg_set_copy(out, MSG_MERGED_DURABLE_ROWS, json_obj_get(held, "merged_durable_rows"));
    if (msg_truthy(json_obj_get(held, "retired_durable_rows"))) {
        const json_t *rows = json_obj_get(held, "retired_durable_rows");
        json_t *list = json_array();
        for (size_t i = 0; i < json_len(rows); i++) json_append(list, msg_held_dict(json_get(rows, i)));
        json_set(out, MSG_RETIRED_DURABLE_ROWS, list);
    }
    if (msg_truthy(json_obj_get(held, "tool_call_ids")))
        msg_set_copy(out, "tool_call_ids", json_obj_get(held, "tool_call_ids"));
    msg_set_true_if(out, MSG_OWN_ROW, json_obj_get(held, "own_row"));
    msg_set_true_if(out, MSG_RETIRED_ROW, json_obj_get(held, "retired_row"));
    return out;
}

/* PoP: w30_messages_29cf8f_p1_message_dict @ persistence/sqlite/conversation/messages.py:_message_dict */
json_t *w30_messages_29cf8f_p1_message_dict(const json_t *in) {
    const json_t *message = json_obj_get(in, "message");
    if (!message || message->type != JSON_OBJECT) return json_object();
    return msg_message_dict(message);
}

/* PoP: w30_messages_29cf8f_p1_held_dict @ persistence/sqlite/conversation/messages.py:_held_dict */
json_t *w30_messages_29cf8f_p1_held_dict(const json_t *in) {
    const json_t *held = json_obj_get(in, "held");
    if (!held || held->type != JSON_OBJECT) return json_object();
    return msg_held_dict(held);
}

/* PoP: w30_messages_29cf8f_p1_pruned_original @ persistence/sqlite/conversation/messages.py:_pruned_original */
json_t *w30_messages_29cf8f_p1_pruned_original(const json_t *in) {
    const json_t *before_in = json_obj_get(in, "before");
    const json_t *original = json_obj_get(in, "original");
    json_t *before, *overlay;
    if (!before_in || before_in->type != JSON_OBJECT) before_in = json_object();
    if (!original || original->type != JSON_OBJECT) original = json_object();
    before = msg_message_dict(before_in);
    json_obj_del(before, "_row_id");
    json_obj_del(before, MSG_DB_ROW_SNAPSHOT);
    overlay = msg_held_dict(original);
    for (size_t i = 0; i < json_object_size(overlay); i++) {
        const char *key = json_object_get_key_at(overlay, i);
        json_set(before, key, json_copy(json_object_get_at(overlay, i)));
    }
    json_free(overlay);
    return before;
}

/* A stored row dict as its TranscriptRow wire projection. */
static json_t *msg_transcript_row(const json_t *row) {
    json_t *out = json_object();
    if (!row || row->type != JSON_OBJECT) return out;
    json_set(out, "id", msg_number_of(json_obj_get(row, "id")));
    json_set(out, "session_id", json_string(json_get_str(row, "session_id", "")));
    json_set(out, "visibility", w30_messages_29cf8f_p1_visibility(row));
    json_set(out, "observed", json_bool(msg_truthy(json_obj_get(row, "observed")) ? true : false));
    {
        const json_t *flag = json_obj_get(row, "_compressed_summary");
        json_set(out, "compressed_summary",
                 json_bool((flag && flag->type == JSON_BOOL && flag->bool_val) ? true : false));
    }
    for (size_t i = 0; i < MSG_ARRAY_LEN(MSG_WIRE_FIELDS); i++) {
        const char *name = MSG_WIRE_FIELDS[i];
        json_set(out, name, msg_copy_or_null(json_obj_get(row, name)));
    }
    return out;
}

/* PoP: w30_messages_29cf8f_p1_transcript_row @ persistence/sqlite/conversation/messages.py:_transcript_row */
json_t *w30_messages_29cf8f_p1_transcript_row(const json_t *in) {
    return msg_transcript_row(json_obj_get(in, "row"));
}

/* PoP: w30_messages_29cf8f_p1_sidecar @ persistence/sqlite/conversation/messages.py:_sidecar */
json_t *w30_messages_29cf8f_p1_sidecar(const json_t *in) {
    const json_t *value = json_obj_get(in, "value");
    json_t *parsed;
    if (!value || value->type != JSON_STRING) return value ? json_copy(value) : json_null();
    parsed = json_parse(value->str_val, NULL);
    if (!parsed) return json_string(value->str_val);
    return parsed;
}

/* PoP: w30_messages_29cf8f_p1_wire_fields @ persistence/sqlite/conversation/messages.py:_MESSAGE_OPTIONALS,_WIRE_FIELDS,_SIDECARS */
json_t *w30_messages_29cf8f_p1_wire_fields(const json_t *in) {
    json_t *out = json_object();
    json_t *optionals = json_array(), *wire = json_array(), *sidecars = json_array();
    (void)in;
    for (size_t i = 0; i < MSG_ARRAY_LEN(MSG_MESSAGE_OPTIONALS); i++) json_append(optionals, json_string(MSG_MESSAGE_OPTIONALS[i]));
    for (size_t i = 0; i < MSG_ARRAY_LEN(MSG_WIRE_FIELDS); i++) json_append(wire, json_string(MSG_WIRE_FIELDS[i]));
    for (size_t i = 0; i < MSG_ARRAY_LEN(MSG_SIDECARS); i++) json_append(sidecars, json_string(MSG_SIDECARS[i]));
    json_set(out, "message_optionals", optionals);
    json_set(out, "wire_fields", wire);
    json_set(out, "sidecars", sidecars);
    return out;
}

/* ── gate p2: row stamps and the write path's pure decisions ──────────── */

/* Look up ``mapping[str(id)]``; Python's int keys are JSON object key strings. */
static const json_t *msg_map_get_int(const json_t *mapping, long long id) {
    if (!mapping || mapping->type != JSON_OBJECT) return NULL;
    for (size_t i = 0; i < json_object_size(mapping); i++) {
        const char *key = json_object_get_key_at(mapping, i);
        if (key && strtoll(key, NULL, 10) == id) return json_object_get_at(mapping, i);
    }
    return NULL;
}

/* A stored row dict as its typed MessageRecord (reactions are not metadata). */
static json_t *msg_message_record(const json_t *row) {
    json_t *out = json_object();
    json_t *tool_calls = json_obj_get(row, "tool_calls");
    json_t *metadata = json_obj_get(row, "display_metadata");
    json_set(out, "id", msg_number_of(json_obj_get(row, "id")));
    json_set(out, "role", json_string(json_get_str(row, "role", "")));
    json_set(out, "content", msg_copy_or_null(json_obj_get(row, "content")));
    json_set(out, "timestamp", json_number(json_get_num(row, "timestamp", 0.0)));
    json_set(out, "tool_calls", msg_truthy(tool_calls) ? json_copy(tool_calls) : json_array());
    json_set(out, "tool_name", msg_copy_or_null(json_obj_get(row, "tool_name")));
    json_set(out, "tool_call_id", msg_copy_or_null(json_obj_get(row, "tool_call_id")));
    json_set(out, "display_kind", msg_copy_or_null(json_obj_get(row, "display_kind")));
    json_set(out, "compressed_summary", json_bool(msg_truthy(json_obj_get(row, "_compressed_summary")) ? true : false));
    json_set(out, "visibility", w30_messages_29cf8f_p1_visibility(row));
    json_set(out, "api_content", msg_copy_or_null(json_obj_get(row, "api_content")));
    if (metadata && metadata->type == JSON_OBJECT) {
        json_t *filtered = json_object();
        for (size_t i = 0; i < json_object_size(metadata); i++) {
            const char *key = json_object_get_key_at(metadata, i);
            if (key && !strcmp(key, "reactions")) continue;
            json_set(filtered, key, json_copy(json_object_get_at(metadata, i)));
        }
        if (json_object_size(filtered) == 0) { json_free(filtered); json_set(out, "display_metadata", json_null()); }
        else json_set(out, "display_metadata", filtered);
    } else {
        json_set(out, "display_metadata", msg_copy_or_null(metadata));
    }
    json_set(out, "platform_message_id", msg_copy_or_null(json_obj_get(row, "platform_message_id")));
    json_set(out, "observed", json_bool(msg_truthy(json_obj_get(row, "observed")) ? true : false));
    json_set(out, "effect_disposition", msg_copy_or_null(json_obj_get(row, "effect_disposition")));
    json_set(out, "finish_reason", msg_copy_or_null(json_obj_get(row, "finish_reason")));
    json_set(out, "reasoning", msg_copy_or_null(json_obj_get(row, "reasoning")));
    json_set(out, "reasoning_content", msg_copy_or_null(json_obj_get(row, "reasoning_content")));
    for (size_t i = 0; i < MSG_ARRAY_LEN(MSG_SIDECARS); i++) {
        const char *name = MSG_SIDECARS[i];
        json_t *raw = json_obj_get(row, name);
        json_t *parsed;
        if (!raw || raw->type != JSON_STRING) { json_set(out, name, msg_copy_or_null(raw)); continue; }
        parsed = json_parse(raw->str_val, NULL);
        json_set(out, name, parsed ? parsed : json_string(raw->str_val));
    }
    json_set(out, "message_uid", msg_copy_or_null(json_obj_get(row, "message_uid")));
    return out;
}

/* PoP: w30_messages_29cf8f_p2_message_record @ persistence/sqlite/conversation/messages.py:_message_record */
json_t *w30_messages_29cf8f_p2_message_record(const json_t *in) {
    const json_t *row = json_obj_get(in, "row");
    if (!row || row->type != JSON_OBJECT) return json_object();
    return msg_message_record(row);
}

/* The row id -> stored row map the stamps consult, keyed by int. */
static double msg_stored_timestamp(const json_t *stored, long long row_id, int *found) {
    const json_t *row = msg_map_get_int(stored, row_id);
    const json_t *ts = row ? json_obj_get(row, "timestamp") : NULL;
    *found = ts && ts->type == JSON_NUMBER;
    return *found ? ts->num_val : 0.0;
}

static const char *msg_stored_uid(const json_t *stored, long long row_id) {
    const json_t *row = msg_map_get_int(stored, row_id);
    return row ? json_get_str(row, "message_uid", NULL) : NULL;
}

/* PoP: w30_messages_29cf8f_p2_stamp @ persistence/sqlite/conversation/messages.py:_stamp */
json_t *w30_messages_29cf8f_p2_stamp(const json_t *in) {
    const json_t *msg = json_obj_get(in, "msg");
    const json_t *stored = json_obj_get(in, "stored");
    const json_t *ts;
    const char *uid;
    long long row_id;
    json_t *out;
    if (!msg || msg->type != JSON_OBJECT) msg = json_object();
    row_id = msg_int_value(json_obj_get(msg, "_row_id"));
    out = json_object();
    json_set(out, "id", json_number((double)row_id));
    ts = json_obj_get(msg, "timestamp");
    if (ts && ts->type == JSON_NUMBER) {
        json_set(out, "timestamp", json_number(ts->num_val));
    } else {
        int found = 0;
        double fallback = msg_stored_timestamp(stored, row_id, &found);
        json_set(out, "timestamp", json_number(fallback));
    }
    json_set(out, "db_row_snapshot", msg_copy_or_null(json_obj_get(msg, MSG_DB_ROW_SNAPSHOT)));
    uid = json_get_str(msg, "message_uid", NULL);
    if (!uid || !uid[0]) uid = msg_stored_uid(stored, row_id);
    if (!uid) uid = "";
    json_set(out, "message_uid", json_string(uid));
    json_set(out, "tool_call_uids", msg_copy_or_null(json_obj_get(msg, "_tool_call_uids")));
    json_set(out, "tool_call_uid", msg_copy_or_null(json_obj_get(msg, "_tool_call_uid")));
    return out;
}

/* PoP: w30_messages_29cf8f_p2_append_chunk @ persistence/sqlite/conversation/messages.py:_append_chunk */
json_t *w30_messages_29cf8f_p2_append_chunk(const json_t *in) {
    const json_t *messages = json_obj_get(in, "messages");
    const json_t *active = json_obj_get(in, "active_row");
    json_t *out = json_object();
    json_t *copies = json_array();
    size_t count = json_len(messages);
    int have_restored = 0;
    long long restored_id = 0;
    if (count == 0) {
        json_set(out, "inserted", json_number(0));
        json_set(out, "messages", copies);
        json_set(out, "restored", json_null());
        return out;
    }
    for (size_t i = 0; i < count; i++) {
        json_t *msg = json_copy(json_get(messages, i));
        if (msg_truthy(json_obj_get(msg, MSG_RESTORED_FROM_STORE)) &&
            msg_truthy(json_obj_get(msg, MSG_DB_PERSISTED_MARKER)) && !json_has(msg, "_row_id")) {
            const json_t *id = active ? json_obj_get(active, "id") : NULL;
            json_set(msg, "_row_id", msg_number_of(id));
            msg_set_not_none(msg, "timestamp", active ? json_obj_get(active, "timestamp") : NULL);
            msg_set_not_none(msg, MSG_DB_ROW_SNAPSHOT, active ? json_obj_get(active, "snapshot") : NULL);
            if (!have_restored) { have_restored = 1; restored_id = msg_int_value(id); }
        }
        json_append(copies, msg);
    }
    json_set(out, "inserted", json_number((double)msg_int_value(json_obj_get(in, "inserted"))));
    json_set(out, "messages", copies);
    json_set(out, "restored", have_restored ? json_number((double)restored_id) : json_null());
    return out;
}

/* PoP: w30_messages_29cf8f_p2_row_outcomes @ persistence/sqlite/conversation/messages.py:_row_outcomes */
json_t *w30_messages_29cf8f_p2_row_outcomes(const json_t *in) {
    const json_t *rows = json_obj_get(in, "rows");
    long long count = msg_int_value(json_obj_get(in, "count"));
    size_t n = json_len(rows);
    json_t *out = json_array();
    long long *ids = n ? (long long *)calloc(n, sizeof(long long)) : NULL;
    size_t unique = 0;
    if (!ids && n) return out;
    for (size_t i = 0; i < n; i++) {
        long long id = msg_int_value(json_obj_get(json_get(rows, i), "_row_id"));
        int seen = 0;
        for (size_t j = 0; j < unique; j++) if (ids[j] == id) { seen = 1; break; }
        if (!seen) ids[unique++] = id;
    }
    /* sorted(set(ids), reverse=True)[:count] -> the ``count`` largest ids */
    for (size_t i = 0; i < unique; i++)
        for (size_t j = i + 1; j < unique; j++)
            if (ids[j] > ids[i]) { long long t = ids[i]; ids[i] = ids[j]; ids[j] = t; }
    for (size_t i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        long long row_id = msg_int_value(json_obj_get(row, "_row_id"));
        const json_t *canonical = json_obj_get(row, "_canonical_row");
        const char *disposition;
        json_t *item = json_object();
        int fresh = 0;
        for (long long k = 0; k < count && k < (long long)unique; k++) if (ids[k] == row_id) { fresh = 1; break; }
        if (fresh) disposition = "appended";
        else if (msg_is_not_none(canonical) && !msg_truthy(json_obj_get(canonical, "_metadata_only"))) disposition = "adopted";
        else disposition = "repaired";
        json_set(item, "id", json_number((double)row_id));
        json_set(item, "disposition", json_string(disposition));
        json_set(item, "timestamp", json_number(json_get_num(row, "timestamp", 0.0)));
        json_set(item, "db_row_snapshot", msg_copy_or_null(json_obj_get(row, MSG_DB_ROW_SNAPSHOT)));
        json_set(item, "canonical_row", msg_copy_or_null(canonical));
        json_set(item, "message_uid", msg_copy_or_null(json_obj_get(row, "message_uid")));
        json_set(item, "tool_call_uids", msg_copy_or_null(json_obj_get(row, "_tool_call_uids")));
        json_set(item, "tool_call_uid", msg_copy_or_null(json_obj_get(row, "_tool_call_uid")));
        json_append(out, item);
    }
    free(ids);
    return out;
}

/* PoP: w30_messages_29cf8f_p2_batch_chunks @ persistence/sqlite/conversation/messages.py:append_batch */
json_t *w30_messages_29cf8f_p2_batch_chunks(const json_t *in) {
    const json_t *messages = json_obj_get(in, "messages");
    const json_t *chunk_rows_in = json_obj_get(in, "chunk_rows");
    size_t total = json_len(messages);
    json_t *out = json_object();
    json_t *dicts = json_array();
    json_t *chunks = json_array();
    json_t *stamped = json_array();
    json_t *outcomes = json_array();
    int chunked = msg_is_not_none(chunk_rows_in) && msg_int_value(chunk_rows_in) > 0 &&
                  (size_t)msg_int_value(chunk_rows_in) < total;
    size_t chunk_rows = msg_int_value(chunk_rows_in) > 0 ? (size_t)msg_int_value(chunk_rows_in) : 0;
    for (size_t i = 0; i < total; i++) {
        const json_t *raw = json_get(messages, i);
        json_append(dicts, msg_message_dict(raw && raw->type == JSON_OBJECT ? raw : json_object()));
    }
    if (chunked) {
        for (size_t i = 0; i < total; i++)
            json_append(stamped, msg_copy_or_null(json_obj_get(json_get(dicts, i), "content")));
        for (size_t start = 0; start < total; start += chunk_rows) {
            json_t *chunk = json_array();
            size_t end = start + chunk_rows < total ? start + chunk_rows : total;
            for (size_t i = start; i < end; i++) {
                json_t *msg = json_copy(json_get(dicts, i));
                if (!json_has(msg, "_row_id")) json_set(msg, "_row_id", json_number((double)(100 + (i - start))));
                if (!msg_is_not_none(json_obj_get(msg, "timestamp"))) json_set(msg, "timestamp", json_number(1.0));
                json_append(chunk, msg);
            }
            json_append(chunks, chunk);
        }
    } else {
        json_t *chunk = json_array();
        for (size_t i = 0; i < total; i++) {
            json_t *msg = json_copy(json_get(dicts, i));
            if (!json_has(msg, "_row_id")) json_set(msg, "_row_id", json_number((double)(100 + i)));
            if (!msg_is_not_none(json_obj_get(msg, "timestamp"))) json_set(msg, "timestamp", json_number(1.0));
            json_append(chunk, msg);
        }
        json_append(chunks, chunk);
    }
    for (size_t c = 0; c < json_len(chunks); c++) {
        /* the recording write returns 0 inserted rows: nothing fresh, no
         * canonical row on a plain dict, so every row is REPAIRED */
        size_t rows_in_chunk = json_len(json_get(chunks, c));
        for (size_t r = 0; r < rows_in_chunk; r++) json_append(outcomes, json_string("repaired"));
    }
    json_set(out, "stamped", stamped);
    json_set(out, "chunks", chunks);
    json_set(out, "outcomes", outcomes);
    return out;
}

/* PoP: w30_messages_29cf8f_p2_replace_rows @ persistence/sqlite/conversation/messages.py:replace_transcript */
json_t *w30_messages_29cf8f_p2_replace_rows(const json_t *in) {
    const json_t *messages = json_obj_get(in, "messages");
    const json_t *stored = json_obj_get(in, "stored");
    const json_t *row_ids = json_obj_get(in, "row_ids");
    const char *mode = json_get_str(in, "mode", "replace");
    size_t total = json_len(messages);
    json_t *out = json_object();
    json_t *rows = json_array();
    json_t *replace = json_object();
    json_t **dicts = total ? (json_t **)calloc(total, sizeof(json_t *)) : NULL;
    size_t kept = 0;
    if (!dicts && total) return out;
    for (size_t i = 0; i < total; i++) {
        const json_t *raw = json_get(messages, i);
        const json_t *content;
        const json_t *mapped;
        long long assigned;
        dicts[i] = msg_message_dict(raw && raw->type == JSON_OBJECT ? raw : json_object());
        json_obj_del(dicts[i], MSG_DB_ROW_SNAPSHOT);
        content = json_obj_get(dicts[i], "content");
        mapped = (row_ids && content && content->type == JSON_STRING) ? json_obj_get(row_ids, content->str_val) : NULL;
        assigned = msg_is_not_none(mapped) ? msg_int_value(mapped) : (long long)(900 + i);
        json_set(dicts[i], "_row_id", json_number((double)assigned));
        if (!msg_is_not_none(json_obj_get(dicts[i], "timestamp")) || !msg_truthy(json_obj_get(dicts[i], "message_uid")))
            kept = 1;
    }
    for (size_t i = 0; i < total; i++) {
        json_t *req = json_object();
        json_t *single_stored = json_object();
        json_set(req, "msg", json_copy(dicts[i]));
        if (kept && stored && stored->type == JSON_ARRAY) {
            for (size_t k = 0; k < json_len(stored); k++) {
                char key[32];
                snprintf(key, sizeof(key), "%lld", msg_int_value(json_obj_get(json_get(stored, k), "id")));
                json_set(single_stored, key, json_copy(json_get(stored, k)));
            }
        }
        json_set(req, "stored", single_stored);
        json_append(rows, w30_messages_29cf8f_p2_stamp(req));
        json_free(req);
    }
    json_set(replace, "active_only", json_bool(!strcmp(mode, "preserve_existing_archive") ? true : false));
    json_set(replace, "archive_dropped", json_bool(!strcmp(mode, "archive_dropped") ? true : false));
    json_set(replace, "reject_active_turn_lease", json_bool(msg_truthy(json_obj_get(in, "reject_active_turn_lease")) ? true : false));
    for (size_t i = 0; i < total; i++) json_free(dicts[i]);
    free(dicts);
    json_set(out, "rows", rows);
    json_set(out, "replace", replace);
    json_set(out, "loaded_stored", json_number(kept ? 1 : 0));
    return out;
}

/* PoP: w30_messages_29cf8f_p2_reactions @ persistence/sqlite/conversation/messages.py:set_reaction */
json_t *w30_messages_29cf8f_p2_reactions(const json_t *in) {
    const json_t *reactions = json_obj_get(in, "reactions");
    json_t *out;
    if (!msg_is_not_none(reactions)) return json_null();
    out = json_array();
    for (size_t i = 0; i < json_len(reactions); i++) {
        const json_t *row = json_get(reactions, i);
        json_t *item = json_object();
        json_set(item, "emoji", msg_copy_or_null(json_obj_get(row, "emoji")));
        json_set(item, "author", msg_copy_or_null(json_obj_get(row, "author")));
        {
            const json_t *at = json_obj_get(row, "at");
            double value = msg_truthy(at) && at->type == JSON_NUMBER ? at->num_val : 0.0;
            json_set(item, "at", json_number(value));
        }
        json_set(item, "seen", json_bool(msg_truthy(json_obj_get(row, "seen")) ? true : false));
        json_append(out, item);
    }
    return out;
}

/* PoP: w30_messages_29cf8f_p2_unseen_reactions @ persistence/sqlite/conversation/messages.py:take_unseen_reactions */
json_t *w30_messages_29cf8f_p2_unseen_reactions(const json_t *in) {
    const json_t *pending = json_obj_get(in, "pending");
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(pending); i++) {
        const json_t *row = json_get(pending, i);
        json_t *item = json_object();
        json_set(item, "row_id", msg_number_of(json_obj_get(row, "row_id")));
        json_set(item, "role", json_string(json_get_str(row, "role", "")));
        json_set(item, "emoji", json_string(json_get_str(row, "emoji", "")));
        json_set(item, "text", json_string(json_get_str(row, "text", "")));
        json_append(out, item);
    }
    return out;
}

/* PoP: w30_messages_29cf8f_p2_cooldown_snapshot @ persistence/sqlite/conversation/messages.py:read_compression_cooldown_snapshot */
json_t *w30_messages_29cf8f_p2_cooldown_snapshot(const json_t *in) {
    const json_t *row = json_obj_get(in, "row");
    json_t *out = json_object();
    if (!row || row->type != JSON_OBJECT) row = json_object();
    if (!msg_truthy(json_obj_get(row, "session_exists"))) {
        json_set(out, "present", json_bool(false));
        json_set(out, "until", json_null());
        json_set(out, "error", json_null());
        return out;
    }
    json_set(out, "present", json_bool(true));
    json_set(out, "until", msg_copy_or_null(json_obj_get(row, "cooldown_until")));
    json_set(out, "error", msg_copy_or_null(json_obj_get(row, "error")));
    return out;
}

/* PoP: w30_messages_29cf8f_p2_compression_cooldown @ persistence/sqlite/conversation/messages.py:read_compression_cooldown */
json_t *w30_messages_29cf8f_p2_compression_cooldown(const json_t *in) {
    const json_t *snapshot = json_obj_get(in, "snapshot");
    const json_t *until = snapshot ? json_obj_get(snapshot, "until") : NULL;
    double at = json_get_num(in, "at", 0.0);
    json_t *out;
    if (!msg_is_not_none(until)) return json_null();
    if (json_get_num(snapshot, "until", 0.0) <= at) return json_null();
    out = json_object();
    json_set(out, "until", json_number(json_get_num(snapshot, "until", 0.0)));
    json_set(out, "error", msg_copy_or_null(json_obj_get(snapshot, "error")));
    return out;
}

/* PoP: w30_messages_29cf8f_p2_storage_state @ persistence/sqlite/conversation/messages.py:message_storage_state */
json_t *w30_messages_29cf8f_p2_storage_state(const json_t *in) {
    const json_t *mid = json_obj_get(in, "message_id");
    const json_t *row = json_obj_get(in, "row");
    json_t *out;
    if (!msg_truthy(mid)) return json_null();
    if (!msg_is_not_none(row)) return json_null();
    out = json_object();
    json_set(out, "session_id", json_string(json_get_str(row, "session_id", "")));
    json_set(out, "visibility", w30_messages_29cf8f_p1_visibility(row));
    return out;
}

/* ── gate p3: SQL statement text and the read envelopes ───────────────── */

/* The statement text each read hands its connection, byte for byte. */
static const char *const MSG_SQL_LATEST_DIALOGUE =
    "SELECT content FROM messages"
    " WHERE session_id = ? AND role IN ('user', 'assistant')"
    " AND active = 1 AND content IS NOT NULL AND TRIM(content) != ''"
    " ORDER BY id DESC LIMIT 1";
static const char *const MSG_SQL_MESSAGE_STORAGE_STATE =
    "SELECT session_id, active, compacted FROM messages WHERE id = ?";
static const char *const MSG_SQL_TERMINAL_PAYLOADS =
    "SELECT tool_calls FROM messages WHERE role='assistant' AND tool_calls IS NOT NULL "
    "AND tool_calls LIKE '%terminal%' AND timestamp >= ?";
static const char *const MSG_SQL_BLOCKED_PAYLOADS =
    "SELECT tool_call_id, content FROM messages "
    "WHERE role='tool' AND tool_call_id IS NOT NULL AND timestamp >= ? "
    "AND (content LIKE '%BLOCKED%' OR content LIKE '%approval%')";
static const char *const MSG_SQL_HAS_MESSAGES_TABLE =
    "SELECT 1 FROM sqlite_master WHERE type='table' AND name='messages'";
static const char *const MSG_SQL_HAS_ANY_MESSAGE = "SELECT 1 FROM messages LIMIT 1";

/* _store/rules.SPILLOVER_SQL_FILTER: instr(), not LIKE (structured content is
 * stored behind a NUL sentinel and LIKE stops at NUL). */
static const char *const MSG_SPILLOVER_SQL_FILTER =
    "(instr(content, 'spillover') > 0 OR instr(api_content, 'spillover') > 0"
    " OR instr(content, 'cache/pruned/') > 0 OR instr(api_content, 'cache/pruned/') > 0)";
#define MSG_SPILLOVER_LIMIT 20001

/* PoP: w30_messages_29cf8f_p3_sql_latest_dialogue @ persistence/sqlite/conversation/messages.py:latest_dialogue_message */
json_t *w30_messages_29cf8f_p3_sql_latest_dialogue(const json_t *in) {
    const json_t *row = json_obj_get(in, "row");
    json_t *out = json_object();
    json_t *params = json_array();
    json_append(params, json_string(json_get_str(in, "session_id", "")));
    json_set(out, "sql", json_string(MSG_SQL_LATEST_DIALOGUE));
    json_set(out, "params", params);
    json_set(out, "out", msg_is_not_none(row) ? msg_copy_or_null(json_get(row, 0)) : json_null());
    return out;
}

/* PoP: w30_messages_29cf8f_p3_sql_message_storage_state @ persistence/sqlite/conversation/messages.py:message_storage_state */
json_t *w30_messages_29cf8f_p3_sql_message_storage_state(const json_t *in) {
    json_t *out = json_object();
    json_t *params = json_array();
    json_append(params, msg_number_of(json_obj_get(in, "message_id")));
    json_set(out, "sql", json_string(MSG_SQL_MESSAGE_STORAGE_STATE));
    json_set(out, "params", params);
    return out;
}

/* PoP: w30_messages_29cf8f_p3_sql_terminal_payloads @ persistence/sqlite/conversation/messages.py:terminal_tool_call_payloads */
json_t *w30_messages_29cf8f_p3_sql_terminal_payloads(const json_t *in) {
    const json_t *rows = json_obj_get(in, "rows");
    json_t *out = json_object();
    json_t *params = json_array();
    json_t *values = json_array();
    json_append(params, json_number(json_get_num(in, "since_ts", 0.0)));
    for (size_t i = 0; i < json_len(rows); i++)
        json_append(values, msg_copy_or_null(json_get(json_get(rows, i), 0)));
    json_set(out, "sql", json_string(MSG_SQL_TERMINAL_PAYLOADS));
    json_set(out, "params", params);
    json_set(out, "out", values);
    return out;
}

/* PoP: w30_messages_29cf8f_p3_sql_blocked_payloads @ persistence/sqlite/conversation/messages.py:blocked_tool_result_payloads */
json_t *w30_messages_29cf8f_p3_sql_blocked_payloads(const json_t *in) {
    const json_t *rows = json_obj_get(in, "rows");
    json_t *out = json_object();
    json_t *params = json_array();
    json_t *values = json_array();
    json_append(params, json_number(json_get_num(in, "since_ts", 0.0)));
    for (size_t i = 0; i < json_len(rows); i++) {
        const json_t *pair = json_get(rows, i);
        json_t *item = json_array();
        json_append(item, msg_copy_or_null(json_get(pair, 0)));
        json_append(item, msg_copy_or_null(json_get(pair, 1)));
        json_append(values, item);
    }
    json_set(out, "sql", json_string(MSG_SQL_BLOCKED_PAYLOADS));
    json_set(out, "params", params);
    json_set(out, "out", values);
    return out;
}

/* PoP: w30_messages_29cf8f_p3_sql_has_any_message @ persistence/sqlite/conversation/messages.py:has_any_message */
json_t *w30_messages_29cf8f_p3_sql_has_any_message(const json_t *in) {
    int has_table = msg_truthy(json_obj_get(in, "table"));
    int has_any = msg_truthy(json_obj_get(in, "any"));
    json_t *out = json_object();
    json_t *statements = json_array();
    json_append(statements, json_string(MSG_SQL_HAS_MESSAGES_TABLE));
    if (has_table) json_append(statements, json_string(MSG_SQL_HAS_ANY_MESSAGE));
    json_set(out, "statements", statements);
    json_set(out, "out", json_bool(has_table && has_any ? true : false));
    return out;
}

/* PoP: w30_messages_29cf8f_p3_sql_spillover_reference @ persistence/sqlite/conversation/messages.py:spillover_reference_rows */
json_t *w30_messages_29cf8f_p3_sql_spillover_reference(const json_t *in) {
    const json_t *rows = json_obj_get(in, "rows");
    json_t *out = json_object();
    json_t *values = json_array();
    size_t n = json_len(rows);
    char sql[1024];
    if (n > (size_t)MSG_SPILLOVER_LIMIT) n = (size_t)MSG_SPILLOVER_LIMIT;
    for (size_t i = 0; i < n; i++) {
        const json_t *pair = json_get(rows, i);
        json_t *item = json_array();
        json_append(item, msg_copy_or_null(json_get(pair, 0)));
        json_append(item, msg_copy_or_null(json_get(pair, 1)));
        json_append(values, item);
    }
    snprintf(sql, sizeof(sql), "SELECT content, api_content FROM messages WHERE %s LIMIT %d",
             MSG_SPILLOVER_SQL_FILTER, MSG_SPILLOVER_LIMIT);
    json_set(out, "sql", json_string(sql));
    json_set(out, "out", values);
    return out;
}

/* PoP: w30_messages_29cf8f_p3_sql_filter @ persistence/sqlite/conversation/_store/rules.py:SPILLOVER_SQL_FILTER */
json_t *w30_messages_29cf8f_p3_sql_filter(const json_t *in) {
    json_t *out = json_object();
    (void)in;
    json_set(out, "spillover_filter", json_string(MSG_SPILLOVER_SQL_FILTER));
    json_set(out, "spillover_limit", json_number(MSG_SPILLOVER_LIMIT));
    return out;
}

/* A list of stored row dicts as MessageRecord envelopes. */
static json_t *msg_record_list(const json_t *rows) {
    json_t *items = json_array();
    for (size_t i = 0; i < json_len(rows); i++) json_append(items, msg_message_record(json_get(rows, i)));
    return items;
}

/* PoP: w30_messages_29cf8f_p3_timeline_page @ persistence/sqlite/conversation/messages.py:read_session_timeline */
json_t *w30_messages_29cf8f_p3_timeline_page(const json_t *in) {
    const json_t *result = json_obj_get(in, "result");
    const json_t *entries = result ? json_obj_get(result, "entries") : NULL;
    const json_t *page = result ? json_obj_get(result, "pagination") : NULL;
    json_t *out = json_object();
    json_t *items = json_array();
    for (size_t i = 0; i < json_len(entries); i++) {
        const json_t *entry = json_get(entries, i);
        json_t *item = json_object();
        json_set(item, "row_id", msg_number_of(json_obj_get(entry, "row_id")));
        json_set(item, "preview", json_string(json_get_str(entry, "preview", "")));
        json_set(item, "timestamp", json_number(json_get_num(entry, "timestamp", 0.0)));
        json_append(items, item);
    }
    json_set(out, "entries", items);
    json_set(out, "limit", msg_number_of(json_obj_get(page, "limit")));
    json_set(out, "after_row_id", msg_number_of(json_obj_get(page, "after_row_id")));
    json_set(out, "returned", msg_number_of(json_obj_get(page, "returned")));
    json_set(out, "total", msg_number_of(json_obj_get(page, "total")));
    json_set(out, "has_more", json_bool(msg_truthy(json_obj_get(page, "has_more")) ? true : false));
    json_set(out, "next_cursor", msg_copy_or_null(json_obj_get(page, "next_cursor")));
    return out;
}

/* PoP: w30_messages_29cf8f_p3_prompt_page @ persistence/sqlite/conversation/messages.py:read_prompt_page */
json_t *w30_messages_29cf8f_p3_prompt_page(const json_t *in) {
    const json_t *result = json_obj_get(in, "result");
    const json_t *messages = result ? json_obj_get(result, "messages") : NULL;
    const json_t *page = result ? json_obj_get(result, "pagination") : NULL;
    json_t *out = json_object();
    json_t *items = json_array();
    if (!msg_is_not_none(result)) return json_null();
    for (size_t i = 0; i < json_len(messages); i++)
        json_append(items, msg_transcript_row(json_get(messages, i)));
    json_set(out, "messages", items);
    json_set(out, "row_id", msg_number_of(json_obj_get(page, "row_id")));
    json_set(out, "limit", msg_number_of(json_obj_get(page, "limit")));
    json_set(out, "returned", msg_number_of(json_obj_get(page, "returned")));
    json_set(out, "offset", msg_number_of(json_obj_get(page, "offset")));
    json_set(out, "total", msg_number_of(json_obj_get(page, "total")));
    json_set(out, "has_older", json_bool(msg_truthy(json_obj_get(page, "has_older")) ? true : false));
    json_set(out, "has_newer", json_bool(msg_truthy(json_obj_get(page, "has_newer")) ? true : false));
    return out;
}

/* PoP: w30_messages_29cf8f_p3_anchored_window @ persistence/sqlite/conversation/messages.py:read_around */
json_t *w30_messages_29cf8f_p3_anchored_window(const json_t *in) {
    const json_t *around = json_obj_get(in, "around");
    json_t *out = json_object();
    json_set(out, "window", msg_record_list(around ? json_obj_get(around, "window") : NULL));
    json_set(out, "before", msg_number_of(json_obj_get(around, "messages_before")));
    json_set(out, "after", msg_number_of(json_obj_get(around, "messages_after")));
    return out;
}

/* PoP: w30_messages_29cf8f_p3_anchored_view @ persistence/sqlite/conversation/messages.py:read_anchored_view */
json_t *w30_messages_29cf8f_p3_anchored_view(const json_t *in) {
    const json_t *view = json_obj_get(in, "view");
    json_t *out = json_object();
    json_set(out, "window", msg_record_list(view ? json_obj_get(view, "window") : NULL));
    json_set(out, "before", msg_number_of(json_obj_get(view, "messages_before")));
    json_set(out, "after", msg_number_of(json_obj_get(view, "messages_after")));
    json_set(out, "bookend_start", msg_record_list(view ? json_obj_get(view, "bookend_start") : NULL));
    json_set(out, "bookend_end", msg_record_list(view ? json_obj_get(view, "bookend_end") : NULL));
    return out;
}

/* PoP: w30_messages_29cf8f_p3_pr_url_hits @ persistence/sqlite/conversation/messages.py:find_pr_url_hits */
json_t *w30_messages_29cf8f_p3_pr_url_hits(const json_t *in) {
    const json_t *hits = json_obj_get(in, "hits");
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(hits); i++) {
        const json_t *hit = json_get(hits, i);
        json_t *item = json_object();
        json_set(item, "session_id", json_string(json_get_str(hit, "session_id", "")));
        json_set(item, "content", json_string(json_get_str(hit, "content", "")));
        json_append(out, item);
    }
    return out;
}

/* PoP: w30_messages_29cf8f_p3_handoff_map @ persistence/sqlite/conversation/messages.py:transcript_handoff_rows */
json_t *w30_messages_29cf8f_p3_handoff_map(const json_t *in) {
    const json_t *found = json_obj_get(in, "found");
    json_t *out = json_object();
    for (size_t i = 0; i < json_object_size(found); i++) {
        const char *key = json_object_get_key_at(found, i);
        const json_t *rows = json_object_get_at(found, i);
        json_t *items = json_array();
        for (size_t r = 0; r < json_len(rows); r++) json_append(items, msg_transcript_row(json_get(rows, r)));
        if (key) json_set(out, key, items); else json_free(items);
    }
    return out;
}

/* PoP: w30_messages_29cf8f_p3_watermark @ persistence/sqlite/conversation/messages.py:active_watermark */
json_t *w30_messages_29cf8f_p3_watermark(const json_t *in) {
    const json_t *last = json_obj_get(in, "last");
    json_t *out = json_object();
    json_set(out, "last_active", msg_truthy(last) ? msg_number_of(last) : json_null());
    return out;
}
