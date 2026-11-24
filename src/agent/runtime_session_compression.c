/*
 * runtime_session_compression.c - real C11 port of the argument projections in upstream Hermes
 * agent/runtime_session_compression.py. The owner's _apply RPC and the SessionDB write stay
 * Python-side; the closed-handoff projection, the receipt re-scoping and the coercion of the
 * compression counters are here.
 * PoP: w19_runtimeses_1b971e_handoff_rows @ agent/runtime_session_compression.py:_handoff_rows
 * PoP: w19_runtimeses_1b971e_cas_rows @ agent/runtime_session_compression.py:archive_and_compact
 * PoP: w19_runtimeses_1b971e_declared_scope_identity @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.declared_scope_identity
 * PoP: w19_runtimeses_1b971e_is_explicit_fork_child @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.is_explicit_fork_child
 * PoP: w19_runtimeses_1b971e_receipt_journal @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin._compression_receipt_journal
 * PoP: w19_runtimeses_1b971e_fallback_streak @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.set_compression_fallback_streak
 * PoP: w19_runtimeses_1b971e_ineffective_count @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.set_compression_ineffective_count
 * PoP: w19_runtimeses_1b971e_recovery_deadline @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.set_compression_recovery_deadline
 * PoP: w19_runtimeses_1b971e_session_readable @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.get_session
 */
#include "noc_w19_runtimeses_1b971e.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* persistence.workers.vocabulary.HANDOFF_MESSAGE_FIELDS (38 keys): the closed worker wire. */
static const char *const HANDOFF_FIELDS[] = {
    "_absorbed_message_uids", "_absorbed_row_ids", "_canonical_content", "_canonical_row",
    "_compressed_summary", "_compressed_summary_has_user_turn", "_db_row_snapshot",
    "_merged_durable_rows", "_merged_turn_prefix", "_restored_from_store",
    "_retired_durable_rows", "_row_id", "_submit_row_session_id", "_tool_call_uid",
    "_tool_call_uids", "_unnamed_durable_rows", "api_content", "codex_message_items",
    "codex_reasoning_items", "content", "data_use", "display_kind", "display_metadata",
    "effect_disposition", "finish_reason", "message_id", "message_uid", "observed",
    "platform_message_id", "reasoning", "reasoning_content", "reasoning_details", "role",
    "timestamp", "token_count", "tool_call_id", "tool_calls", "tool_name", NULL
};

static const json_t *jget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static int in_fields(const char *k, int allow_db_persisted) {
    int i;
    if (allow_db_persisted && k && !strcmp(k, "_db_persisted")) return 1;
    for (i = 0; HANDOFF_FIELDS[i]; i++)
        if (!strcmp(HANDOFF_FIELDS[i], k)) return 1;
    return 0;
}

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

static json_t *error_obj(const char *text) {
    json_t *out = json_object();
    json_set(out, "error", json_string(text ? text : ""));
    return out;
}

static json_t *project(const json_t *m, int allow_db_persisted) {
    json_t *out = json_object();
    size_t i;
    for (i = 0; i < json_object_size(m); i++) {
        const char *k = json_object_get_key_at(m, i);
        if (!in_fields(k, allow_db_persisted)) continue;
        json_set(out, k, json_copy(json_object_get_at(m, i)));
    }
    return out;
}

json_t *w19_runtimeses_1b971e_handoff_rows(const json_t *messages) {
    json_t *out = json_array();
    size_t i;
    if (!messages || messages->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(messages); i++) {
        const json_t *m = json_get(messages, i);
        if (m && m->type == JSON_OBJECT) json_append(out, project(m, 0));
        else json_append(out, m ? json_copy(m) : json_null());
    }
    return out;
}

json_t *w19_runtimeses_1b971e_cas_rows(const json_t *held) {
    json_t *out = json_array();
    size_t i;
    if (!held || held->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(held); i++) {
        const json_t *m = json_get(held, i);
        if (m && m->type == JSON_OBJECT) json_append(out, project(m, 1));
    }
    return out;
}

json_t *w19_runtimeses_1b971e_declared_scope_identity(const json_t *identity) {
    if (!identity || identity->type != JSON_ARRAY) return json_null();
    return json_copy(identity);
}

json_t *w19_runtimeses_1b971e_is_explicit_fork_child(const json_t *identity) {
    if (!identity || identity->type != JSON_ARRAY || json_len(identity) == 0) return json_null();
    return json_copy(json_get(identity, 0));
}

json_t *w19_runtimeses_1b971e_receipt_journal(const json_t *candidate, const json_t *result,
                                              const json_t *scope) {
    const json_t *assignment = jget(result, "worker_assignment");
    if (!truthy(assignment)) return candidate ? json_copy(candidate) : json_null();
    if (assignment->type != JSON_OBJECT) return candidate ? json_copy(candidate) : json_null();
    {
        const json_t *parent = json_obj_get(assignment, "parent");
        const char *scope_id = "";
        json_t *out;
        json_t *new_scope;
        if (!parent) return error_obj("KeyError: 'parent'");
        {
            const json_t *sid = jget(scope, "session_id");
            if (sid && sid->type == JSON_STRING && sid->str_val) scope_id = sid->str_val;
        }
        if (!(parent->type == JSON_STRING && parent->str_val && !strcmp(parent->str_val, scope_id)))
            return error_obj("outbox_scope_mismatch");
        out = candidate && candidate->type == JSON_OBJECT ? json_copy(candidate) : json_object();
        new_scope = scope && scope->type == JSON_OBJECT ? json_copy(scope) : json_object();
        json_set(new_scope, "session_id", json_copy(json_obj_get(assignment, "session_id")));
        json_set(out, "scope", new_scope);
        return out;
    }
}

/* Python int(v) for the JSON scalars these counters accept; 1 on success. */
static int py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        char *end = NULL;
        long long x;
        if (!s) return 0;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
        if (!*s) return 0;
        x = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\f' || *end == '\v') end++;
        if (*end != '\0') return 0;
        *out = x;
        return 1;
    }
    return 0;
}

static json_t *clamped_count(const json_t *v) {
    long long n;
    if (!v || v->type == JSON_NULL) return error_obj("TypeError");
    if (!py_int(v, &n)) return error_obj("ValueError");
    if (n < 0) n = 0;
    return json_number((double)n);
}

json_t *w19_runtimeses_1b971e_fallback_streak(const json_t *streak) {
    return clamped_count(streak);  /* max(0, int(streak)) */
}

json_t *w19_runtimeses_1b971e_ineffective_count(const json_t *count) {
    return clamped_count(count);  /* max(0, int(count)) */
}

/* Python float(v) for the scalars a deadline may arrive as; 1 on success. */
static int py_float(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        char *end = NULL;
        double x;
        if (!s) return 0;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
        if (!*s) return 0;
        x = strtod(s, &end);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\f' || *end == '\v') end++;
        if (*end != '\0') return 0;
        *out = x;
        return 1;
    }
    return 0;
}

json_t *w19_runtimeses_1b971e_recovery_deadline(const json_t *deadline) {
    json_t *out = json_object();
    double value = 0.0;
    const json_t *v = truthy(deadline) ? deadline : NULL;
    if (v) {
        if (!py_float(v, &value)) value = 0.0;
        if (value < 0.0) value = 0.0;
    }
    json_set(out, "value", value != 0.0 ? json_number(value) : json_null());
    return out;
}

json_t *w19_runtimeses_1b971e_session_readable(const char *scope_session_id, const char *requested,
                                               const json_t *readable_ids) {
    json_t *out = json_object();
    const char *scope = scope_session_id ? scope_session_id : "";
    const char *want = requested ? requested : "";
    if (!strcmp(want, scope)) {
        json_set(out, "allowed", json_bool(1));
        return out;
    }
    if (readable_ids && readable_ids->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(readable_ids); i++) {
            const json_t *id = json_get(readable_ids, i);
            if (id && id->type == JSON_STRING && id->str_val && !strcmp(id->str_val, want)) {
                json_set(out, "allowed", json_bool(1));
                return out;
            }
        }
    }
    json_free(out);
    return error_obj("permission_denied");
}
