/*
 * compaction.c - C home for persistence/workers/compaction.py.
 * Faithful port of the row-copy helper and the two compression payload builders.
 * PoP: w19_compaction_7f3f57_rows @ persistence/workers/compaction.py:_rows
 * PoP: w19_compaction_7f3f57_compaction_payload @ persistence/workers/compaction.py:WorkerCompaction.payload
 * PoP: w19_compaction_7f3f57_publication_payload @ persistence/workers/compaction.py:WorkerCompressionPublication.payload
 */
#include "noc_w19_compaction_7f3f57.h"

/* [dict(row) for row in rows] - an object row copies key for key; a null/absent
 * sequence is the empty list (the dataclass defaults that carry one). */
static json_t *rows_as_list(json_t *rows) {
    if (!rows || rows->type == JSON_NULL) return json_array();
    json_t *out = json_array();
    if (rows->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(rows); i++) {
        json_t *row = json_get(rows, i);
        json_append(out, (row && row->type == JSON_OBJECT) ? json_copy(row) : json_object());
    }
    return out;
}

/* _rows(rows): None -> None, else [dict(row) for row in rows]. */
json_t *w19_compaction_7f3f57_rows(json_t *rows) {
    if (!rows || rows->type == JSON_NULL) return json_null();
    return rows_as_list(rows);
}

static json_t *opt_object(json_t *v) {
    return (v && v->type == JSON_OBJECT) ? json_copy(v) : json_null();
}

static json_t *opt_sequence(json_t *v) {
    return (v && v->type != JSON_NULL) ? rows_as_list(v) : json_null();
}

/* list(self.covered_ids): the ints are copied as they are stored. */
static json_t *opt_int_list(json_t *v) {
    if (!v || v->type == JSON_NULL) return json_null();
    if (v->type != JSON_ARRAY) return json_array();
    return json_copy(v);
}

static json_t *opt_string(const char *s) {
    return s ? json_string(s) : json_null();
}

/* WorkerCompaction.payload(): the nine keys, in the producer's order. */
json_t *w19_compaction_7f3f57_compaction_payload(
    json_t *messages, json_t *model_config_patch, int has_watermark, long long watermark,
    const char *lock_holder, long long tail_count, json_t *carried_messages,
    int has_carried_messages, json_t *covered_ids, json_t *unresolved_held, json_t *held_for_cas) {
    json_t *o = json_object();
    json_set(o, "messages", w19_compaction_7f3f57_rows(messages));
    json_set(o, "model_config_patch", opt_object(model_config_patch));
    json_set(o, "watermark", has_watermark ? json_number((double)watermark) : json_null());
    json_set(o, "lock_holder", opt_string(lock_holder));
    json_set(o, "tail_count", json_number((double)tail_count));
    /* carried_messages defaults to () - absent reads the empty list, None reads null. */
    json_set(o, "carried_messages",
             has_carried_messages ? w19_compaction_7f3f57_rows(carried_messages) : json_array());
    json_set(o, "covered_ids", opt_int_list(covered_ids));
    json_set(o, "unresolved_held", opt_sequence(unresolved_held));
    json_set(o, "held_for_cas", opt_sequence(held_for_cas));
    return o;
}

/* WorkerCompressionPublication.payload(): the fourteen keys, in the producer's order. */
json_t *w19_compaction_7f3f57_publication_payload(
    const char *child_session_id, const char *source, json_t *messages, const char *model,
    json_t *model_config, const char *system_prompt, const char *cwd, const char *profile_name,
    const char *compression_lock_holder, int require_compression_lease, int require_lease_refresh,
    double lease_ttl_seconds, int has_watermark, long long watermark,
    int has_watermark_ceiling, long long watermark_ceiling) {
    json_t *o = json_object();
    json_set(o, "child_session_id", json_string(child_session_id ? child_session_id : ""));
    json_set(o, "source", json_string(source ? source : ""));
    json_set(o, "messages", w19_compaction_7f3f57_rows(messages));
    json_set(o, "model", opt_string(model));
    json_set(o, "model_config", opt_object(model_config));
    json_set(o, "system_prompt", opt_string(system_prompt));
    json_set(o, "cwd", opt_string(cwd));
    json_set(o, "profile_name", opt_string(profile_name));
    json_set(o, "compression_lock_holder", opt_string(compression_lock_holder));
    json_set(o, "require_compression_lease", json_bool(require_compression_lease ? true : false));
    json_set(o, "require_lease_refresh", json_bool(require_lease_refresh ? true : false));
    json_set(o, "lease_ttl_seconds", json_number(lease_ttl_seconds));
    json_set(o, "watermark", has_watermark ? json_number((double)watermark) : json_null());
    json_set(o, "watermark_ceiling",
             has_watermark_ceiling ? json_number((double)watermark_ceiling) : json_null());
    return o;
}
