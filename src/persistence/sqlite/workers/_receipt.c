/*
 * _receipt.c - C11 port of upstream persistence/sqlite/workers/_receipt.py
 * (gate w26_receipt_cbecab).
 *
 * PoP: w26_receipt_cbecab_validate @ persistence/sqlite/workers/_receipt.py:_worker_append
 * PoP: w26_receipt_cbecab_validate @ persistence/sqlite/workers/_receipt.py:_worker_turn
 * PoP: w26_receipt_cbecab_validate @ persistence/sqlite/workers/_receipt.py:_worker_usage
 * PoP: w26_receipt_cbecab_validate @ persistence/sqlite/workers/_receipt.py:_worker_finish
 * PoP: w26_receipt_cbecab_validate @ persistence/sqlite/workers/_receipt.py:apply_worker_receipt
 * PoP: w26_receipt_cbecab_annotations @ persistence/workers/vocabulary.py:row_annotations
 * PoP: w26_receipt_cbecab_constants @ persistence/workers/vocabulary.py:<worker wire vocabulary>
 *
 * Every handler is a payload gate plus one transaction; the connection, the
 * holder, the epoch fence and the row writes are seams. What is ported is the
 * admission gate itself (which payload shapes each closed operation takes,
 * down to the per-key type rules of a usage report), the row annotations the
 * receipt returns to the worker, and the contract bounds.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_receipt_cbecab.h"

/* WORKER_MESSAGE_FIELDS: the keys a transcript row on the worker wire may carry. */
static const char *const RC_MESSAGE_FIELDS[37] = {
    "_absorbed_message_uids", "_absorbed_row_ids", "_canonical_content", "_canonical_row",
    "_compressed_summary", "_db_row_snapshot", "_merged_durable_rows", "_merged_turn_prefix",
    "_restored_from_store", "_retired_durable_rows", "_row_id", "_submit_row_session_id",
    "_tool_call_uid", "_tool_call_uids", "_unnamed_durable_rows", "api_content",
    "codex_message_items", "codex_reasoning_items", "content", "data_use", "display_kind",
    "display_metadata", "effect_disposition", "finish_reason", "message_id", "message_uid",
    "observed", "platform_message_id", "reasoning", "reasoning_content", "reasoning_details",
    "role", "timestamp", "token_count", "tool_call_id", "tool_calls", "tool_name",
};

/* ROW_ANNOTATION_KEYS, in wire order. */
static const char *const RC_ANNOTATION_KEYS[7] = {
    "_row_id", "timestamp", "_db_row_snapshot", "_canonical_row", "message_uid",
    "_tool_call_uids", "_tool_call_uid",
};

/* _TOKEN_COUNTERS */
static const char *const RC_TOKEN_COUNTERS[5] = {
    "input_tokens", "output_tokens", "cache_read_tokens", "cache_write_tokens", "reasoning_tokens",
};

/* _MODEL_USAGE_FIELDS */
static const char *const RC_MODEL_USAGE_FIELDS[14] = {
    "model", "billing_provider", "billing_base_url", "billing_mode", "input_tokens",
    "output_tokens", "cache_read_tokens", "cache_write_tokens", "reasoning_tokens",
    "estimated_cost_usd", "actual_cost_usd", "cost_status", "cost_source", "api_call_count",
};

static const char *const RC_ROLES[4] = {"user", "assistant", "system", "tool"};

/* Python truthiness of a JSON value. */
static int rc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* set(payload) membership over an object's keys (or an array's elements). */
static int rc_set_has(const json_t *p, const char *key) {
    size_t i, n;
    if (!p || p->type == JSON_NULL) return 0;
    if (p->type == JSON_OBJECT) {
        n = json_object_size(p);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(p, i);
            if (k && !strcmp(k, key)) return 1;
        }
        return 0;
    }
    if (p->type == JSON_ARRAY) {
        n = json_len(p);
        for (i = 0; i < n; i++) {
            const json_t *e = json_get(p, i);
            if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, key)) return 1;
        }
    }
    return 0;
}

/* set(payload) - allowed is empty. */
static int rc_set_subset(const json_t *p, const char *const *allowed, size_t na) {
    size_t i, j, n;
    if (!p || p->type == JSON_NULL) return 1;
    if (p->type == JSON_OBJECT) {
        n = json_object_size(p);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(p, i);
            int hit = 0;
            for (j = 0; j < na; j++) if (k && !strcmp(k, allowed[j])) { hit = 1; break; }
            if (!hit) return 0;
        }
        return 1;
    }
    if (p->type == JSON_ARRAY) {
        n = json_len(p);
        for (i = 0; i < n; i++) {
            const json_t *e = json_get(p, i);
            int hit = 0;
            if (!e || e->type != JSON_STRING || !e->str_val) return 0;
            for (j = 0; j < na; j++) if (!strcmp(e->str_val, allowed[j])) { hit = 1; break; }
            if (!hit) return 0;
        }
        return 1;
    }
    return 0;
}

static int rc_same_set(const json_t *p, const char *const *want, size_t nw) {
    size_t i;
    if (!p || p->type != JSON_OBJECT || json_object_size(p) != nw) return 0;
    for (i = 0; i < nw; i++) if (!rc_set_has(p, want[i])) return 0;
    return 1;
}

/* Python len() of a str: code points. */
static size_t rc_strlen(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (; *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}

/* _text(): a non-empty str of at most 1024 characters. */
static int rc_text_ok(const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val || !v->str_val[0]) return 0;
    return rc_strlen(v->str_val) <= W26_RECEIPT_CBECAB_LEASE_TEXT_MAX;
}

/* type(value) is int / type(value) in (int, float): libjson keeps no int tag,
 * but it does keep bool apart from number, which is what these tests need. */
static int rc_is_int(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int rc_is_number(const json_t *v) { return v && v->type == JSON_NUMBER; }

static json_t *rc_refuse(const char *code) { return json_string(code); }

/* _worker_append: a transcript batch on the receipt's own connection. */
static json_t *rc_append(const json_t *payload, int row_exists) {
    static const char *const ALLOWED[2] = {"messages", "turn_lease_holder"};
    const json_t *messages, *lease;
    size_t i, n;

    if (!payload || payload->type != JSON_OBJECT) return rc_refuse("invalid_params");
    if (!rc_set_subset(payload, ALLOWED, 2) || !rc_set_has(payload, "messages"))
        return rc_refuse("invalid_params");
    messages = json_obj_get(payload, "messages");
    if (!messages || messages->type != JSON_ARRAY
        || json_len(messages) > W26_RECEIPT_CBECAB_TRANSCRIPT_MAX)
        return rc_refuse("invalid_params");
    n = json_len(messages);
    for (i = 0; i < n; i++) {
        const json_t *msg = json_get(messages, i);
        const json_t *role;
        int role_ok = 0;
        size_t j;
        if (!msg || msg->type != JSON_OBJECT) return rc_refuse("invalid_params");
        if (!rc_set_subset(msg, RC_MESSAGE_FIELDS, 37)) return rc_refuse("invalid_params");
        role = json_obj_get(msg, "role");
        for (j = 0; j < 4; j++)
            if (role && role->type == JSON_STRING && role->str_val
                && !strcmp(role->str_val, RC_ROLES[j])) { role_ok = 1; break; }
        if (!role_ok) return rc_refuse("invalid_params");
        /* Existing row annotations can adopt only rows in THIS transcript. */
        if (rc_set_has(msg, "_row_id") && !row_exists) return rc_refuse("permission_denied");
    }
    lease = json_obj_get(payload, "turn_lease_holder");
    if (lease && lease->type != JSON_NULL && !rc_text_ok(lease)) return rc_refuse("invalid_params");
    return json_null();
}

/* _worker_turn: turn.acquire / turn.renew over one lease row. */
static json_t *rc_turn(const json_t *payload) {
    static const char *const KEYS[2] = {"holder", "ttl_seconds"};
    const json_t *ttl;
    if (!rc_same_set(payload, KEYS, 2)) return rc_refuse("invalid_params");
    if (!rc_text_ok(json_obj_get(payload, "holder"))) return rc_refuse("invalid_params");
    ttl = json_obj_get(payload, "ttl_seconds");
    /* type(ttl) not in (float, int): None, a bool or a string never passes. */
    if (!rc_is_number(ttl)) return rc_refuse("invalid_params");
    if (!(ttl->num_val >= 0.1 && ttl->num_val <= 3600.0)) return rc_refuse("invalid_params");
    return json_null();
}

/* The two usage key sets: main accepts the whole model-usage vocabulary plus
 * the delta/clock switches, auxiliary drops the receipt-owned cost columns. */
static int rc_usage_allowed(const char *key, int auxiliary) {
    size_t i;
    static const char *const MAIN_EXTRA[4] = {
        "pricing_version", "absolute", "task", "prompt_cache_clock",
    };
    for (i = 0; i < 14; i++) {
        if (strcmp(RC_MODEL_USAGE_FIELDS[i], key)) continue;
        if (!auxiliary) return 1;
        if (!strcmp(key, "billing_mode") || !strcmp(key, "actual_cost_usd")
            || !strcmp(key, "cost_status") || !strcmp(key, "cost_source")) return 0;
        return 1;
    }
    if (auxiliary) return !strcmp(key, "task");
    for (i = 0; i < 4; i++) if (!strcmp(MAIN_EXTRA[i], key)) return 1;
    return 0;
}

static int rc_is_counter(const char *key) {
    size_t i;
    for (i = 0; i < 5; i++) if (!strcmp(RC_TOKEN_COUNTERS[i], key)) return 1;
    return !strcmp(key, "api_call_count");
}

/* _worker_usage: one usage report, key by key. */
static json_t *rc_usage(const json_t *payload, int auxiliary) {
    size_t i, n;

    if (!payload || payload->type != JSON_OBJECT) return rc_refuse("invalid_params");
    n = json_object_size(payload);
    for (i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(payload, i);
        if (!key) continue;
        if (!auxiliary && !strcmp(key, "source")) continue;   /* stripped before the check */
        if (!rc_usage_allowed(key, auxiliary)) return rc_refuse("invalid_params");
    }
    for (i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(payload, i);
        const json_t *value = json_object_get_at(payload, i);
        int valid;
        if (!key) continue;
        /* the main path removes 'source' before the per-key type rules run */
        if (!auxiliary && !strcmp(key, "source")) continue;
        if (rc_is_counter(key)) {
            valid = rc_is_int(value) && value->num_val >= 0.0
                    && value->num_val <= W26_RECEIPT_CBECAB_TOKEN_MAX;
        } else if (!strcmp(key, "estimated_cost_usd") || !strcmp(key, "actual_cost_usd")) {
            valid = !value || value->type == JSON_NULL
                    || (rc_is_number(value) && value->num_val >= 0.0
                        && value->num_val <= W26_RECEIPT_CBECAB_COST_MAX);
        } else if (!strcmp(key, "absolute")) {
            valid = value && value->type == JSON_BOOL;
        } else {
            valid = !value || value->type == JSON_NULL
                    || (value->type == JSON_STRING && rc_strlen(value->str_val)
                        <= W26_RECEIPT_CBECAB_TEXT_MAX);
        }
        if (!valid) return rc_refuse("invalid_params");
    }
    if (auxiliary) {
        const json_t *task = json_obj_get(payload, "task");
        if (!rc_truthy(task)) return rc_refuse("invalid_params");
    }
    return json_null();
}

/* apply_worker_receipt: the sequence/operation fence in front of every handler. */
static json_t *rc_receipt_params(const json_t *req, const json_t *handlers) {
    const json_t *sequence = json_obj_get(req, "sequence");
    const json_t *operation = json_obj_get(req, "operation");
    const json_t *payload = json_obj_get(req, "payload");
    size_t i, n;

    if (!rc_is_int(sequence) || sequence->num_val < 1.0) return rc_refuse("invalid_params");
    if (!operation || operation->type != JSON_STRING || !operation->str_val)
        return rc_refuse("invalid_params");
    if (!handlers || handlers->type != JSON_ARRAY) return rc_refuse("invalid_params");
    n = json_len(handlers);
    for (i = 0; i < n; i++) {
        const json_t *h = json_get(handlers, i);
        if (h && h->type == JSON_STRING && h->str_val
            && !strcmp(h->str_val, operation->str_val)) break;
    }
    if (i == n) return rc_refuse("invalid_params");
    if (payload) {
        char *encoded = json_serialize(payload);
        size_t len;
        if (!encoded) return rc_refuse("invalid_params");
        len = strlen(encoded);
        free(encoded);
        if (len > (size_t)W26_RECEIPT_CBECAB_PAYLOAD_MAX_BYTES) return rc_refuse("invalid_params");
    }
    return json_null();
}

json_t *w26_receipt_cbecab_validate(const json_t *req) {
    const char *op;
    const json_t *payload;
    int row_exists;

    if (!req || req->type != JSON_OBJECT) return rc_refuse("invalid_params");
    op = json_get_str(req, "op", "");
    payload = json_obj_get(req, "payload");
    row_exists = json_get_bool(req, "row_exists", false);
    if (!op) op = "";

    if (!strcmp(op, "transcript.append")) return rc_append(payload, row_exists);
    if (!strcmp(op, "turn.acquire") || !strcmp(op, "turn.renew")) return rc_turn(payload);
    if (!strcmp(op, "usage.main")) return rc_usage(payload, 0);
    if (!strcmp(op, "usage.auxiliary")) return rc_usage(payload, 1);
    if (!strcmp(op, "execution.finish"))
        return rc_truthy(payload) ? rc_refuse("invalid_params") : json_null();
    if (!strcmp(op, "receipt.params")) return rc_receipt_params(req, json_obj_get(req, "handlers"));
    return rc_refuse("UNKNOWN_OP");
}

json_t *w26_receipt_cbecab_annotations(const json_t *req) {
    const json_t *messages = req ? json_obj_get(req, "messages") : NULL;
    json_t *out = json_array();
    size_t i, j, n;

    if (!messages || messages->type != JSON_ARRAY) return out;
    n = json_len(messages);
    for (i = 0; i < n; i++) {
        const json_t *msg = json_get(messages, i);
        json_t *row = json_object();
        for (j = 0; j < 7; j++) {
            const json_t *v = (msg && msg->type == JSON_OBJECT)
                              ? json_obj_get(msg, RC_ANNOTATION_KEYS[j]) : NULL;
            json_set(row, RC_ANNOTATION_KEYS[j], v ? json_copy(v) : json_null());
        }
        json_append(out, row);
    }
    return out;
}

json_t *w26_receipt_cbecab_constants(void) {
    json_t *o = json_object();
    json_t *fields = json_array();
    json_t *ann = json_array();
    json_t *counters = json_array();
    size_t i;

    for (i = 0; i < 37; i++) json_append(fields, json_string(RC_MESSAGE_FIELDS[i]));
    for (i = 0; i < 7; i++) json_append(ann, json_string(RC_ANNOTATION_KEYS[i]));
    for (i = 0; i < 5; i++) json_append(counters, json_string(RC_TOKEN_COUNTERS[i]));
    json_set(o, "message_fields", fields);
    json_set(o, "annotation_keys", ann);
    json_set(o, "token_counters", counters);
    json_set(o, "transcript_batch_max", json_number((double)W26_RECEIPT_CBECAB_TRANSCRIPT_MAX));
    json_set(o, "payload_max_bytes", json_number((double)W26_RECEIPT_CBECAB_PAYLOAD_MAX_BYTES));
    json_set(o, "token_max", json_number(W26_RECEIPT_CBECAB_TOKEN_MAX));
    json_set(o, "cost_max", json_number(W26_RECEIPT_CBECAB_COST_MAX));
    json_set(o, "text_max", json_number((double)W26_RECEIPT_CBECAB_TEXT_MAX));
    return o;
}
