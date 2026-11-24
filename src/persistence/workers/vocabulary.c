/*
 * vocabulary.c -- real C11 port of the worker family's shared vocabulary:
 *   persistence/workers/vocabulary.py.
 *
 * PoP: w20_vocabulary_975e41_fields @ persistence/workers/vocabulary.py:WORKER_MESSAGE_FIELDS
 * PoP: w20_vocabulary_975e41_contains @ persistence/workers/vocabulary.py:WORKER_MESSAGE_FIELDS
 * PoP: w20_vocabulary_975e41_constants @ persistence/workers/vocabulary.py:CHILD_SEPARATOR
 * PoP: w20_vocabulary_975e41_row_annotations @ persistence/workers/vocabulary.py:row_annotations
 *
 * The module is all constants and one pure list comprehension, so every symbol
 * here is a faithful, complete port: nothing about this module stays upstream.
 */
#include "hermes_json.h"
#include "noc_w20_vocabulary_975e41.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CHILD_SEPARATOR: a child's execution id is the parent's id, this separator,
 * then a random suffix. */
static const char VOC975_CHILD_SEPARATOR[] = "/child:";

/* RECEIPT_PAYLOAD_MAX_BYTES and TRANSCRIPT_BATCH_MAX are ints; they leave as
 * decimal strings so no JSON double rounds them. */
static const char VOC975_RECEIPT_MAX[] = "4194304";
static const char VOC975_BATCH_MAX[] = "1000";

/* _PERSISTENCE_ONLY_FIELDS plus the wire fields WORKER_MESSAGE_FIELDS unions
 * them with (agent/message_metadata.py:42-52). */
static const char *const VOC975_WORKER_MESSAGE_FIELDS[] = {
    "_absorbed_message_uids", "_absorbed_row_ids", "_canonical_content",
    "_canonical_row", "_compressed_summary", "_db_row_snapshot",
    "_merged_durable_rows", "_merged_turn_prefix", "_restored_from_store",
    "_retired_durable_rows", "_row_id", "_submit_row_session_id",
    "_tool_call_uid", "_tool_call_uids", "_unnamed_durable_rows",
    "api_content", "codex_message_items", "codex_reasoning_items", "content",
    "data_use", "display_kind", "display_metadata", "effect_disposition",
    "finish_reason", "message_id", "message_uid", "observed",
    "platform_message_id", "reasoning", "reasoning_content",
    "reasoning_details", "role", "timestamp", "token_count", "tool_call_id",
    "tool_calls", "tool_name",
    NULL
};

/* HANDOFF_MESSAGE_FIELDS = WORKER_MESSAGE_FIELDS | {_compressed_summary_has_user_turn}. */
static const char *const VOC975_HANDOFF_MESSAGE_FIELDS[] = {
    "_absorbed_message_uids", "_absorbed_row_ids", "_canonical_content",
    "_canonical_row", "_compressed_summary",
    "_compressed_summary_has_user_turn", "_db_row_snapshot",
    "_merged_durable_rows", "_merged_turn_prefix", "_restored_from_store",
    "_retired_durable_rows", "_row_id", "_submit_row_session_id",
    "_tool_call_uid", "_tool_call_uids", "_unnamed_durable_rows",
    "api_content", "codex_message_items", "codex_reasoning_items", "content",
    "data_use", "display_kind", "display_metadata", "effect_disposition",
    "finish_reason", "message_id", "message_uid", "observed",
    "platform_message_id", "reasoning", "reasoning_content",
    "reasoning_details", "role", "timestamp", "token_count", "tool_call_id",
    "tool_calls", "tool_name",
    NULL
};

/* The model_config keys a worker may patch. */
static const char *const VOC975_SIDECAR_KEYS[] = {
    "_anthropic_rejected_thinking", "_proactive_prune_cold_credit",
    "_proactive_prune_rearm_tokens", "_usage_anchor",
    NULL
};

/* The model_config keys the worker constructor's upsert accepts. */
static const char *const VOC975_CONSTRUCTOR_CONFIG_KEYS[] = {
    "max_iterations", "max_tokens", "reasoning_config", "yolo_mode",
    NULL
};

/* ROW_ANNOTATION_KEYS is a tuple: order is part of its contract (the operator
 * stamps these on each written message in this order), so it is never sorted. */
static const char *const VOC975_ROW_ANNOTATION_KEYS[] = {
    "_row_id", "timestamp", "_db_row_snapshot", "_canonical_row",
    "message_uid", "_tool_call_uids", "_tool_call_uid",
    NULL
};

/* __all__, in source order. */
static const char *const VOC975_ALL[] = {
    "CHILD_SEPARATOR", "HANDOFF_MESSAGE_FIELDS", "RECEIPT_PAYLOAD_MAX_BYTES",
    "ROW_ANNOTATION_KEYS", "SIDECAR_KEYS", "TRANSCRIPT_BATCH_MAX",
    "WORKER_CONSTRUCTOR_CONFIG_KEYS", "WORKER_MESSAGE_FIELDS",
    "row_annotations",
    NULL
};

typedef struct {
    const char *name;
    const char *const *items;
    int sorted;
} voc975_set_row;

static const voc975_set_row VOC975_SETS[] = {
    {"worker_message_fields",   VOC975_WORKER_MESSAGE_FIELDS,   1},
    {"handoff_message_fields",  VOC975_HANDOFF_MESSAGE_FIELDS,  1},
    {"sidecar_keys",            VOC975_SIDECAR_KEYS,            1},
    {"constructor_config_keys", VOC975_CONSTRUCTOR_CONFIG_KEYS, 1},
    {"row_annotation_keys",     VOC975_ROW_ANNOTATION_KEYS,     0},
    {NULL, NULL, 0}
};

static const voc975_set_row *voc975_find(const char *which) {
    int i;
    if (!which) return NULL;
    for (i = 0; VOC975_SETS[i].name; i++) {
        if (!strcmp(which, VOC975_SETS[i].name)) return &VOC975_SETS[i];
    }
    return NULL;
}

static int voc975_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *voc975_array_of(const char *const *items, int sorted) {
    const char *copy[64];
    json_t *arr = json_array();
    int n = 0, i;
    for (i = 0; items[i]; i++) {
        if (n < (int)(sizeof(copy) / sizeof(copy[0]))) copy[n++] = items[i];
    }
    if (sorted && n > 1) qsort(copy, (size_t)n, sizeof(copy[0]), voc975_cmp);
    for (i = 0; i < n; i++) json_append(arr, json_string(copy[i]));
    return arr;
}

json_t *w20_vocabulary_975e41_fields(const char *which) {
    const voc975_set_row *row = voc975_find(which);
    if (!row) return json_null();
    return voc975_array_of(row->items, row->sorted);
}

int w20_vocabulary_975e41_contains(const char *which, const char *key) {
    const voc975_set_row *row = voc975_find(which);
    int i;
    if (!row || !key) return 0;
    for (i = 0; row->items[i]; i++) {
        if (!strcmp(key, row->items[i])) return 1;
    }
    return 0;
}

json_t *w20_vocabulary_975e41_constants(void) {
    json_t *out = json_object();
    json_set(out, "child_separator", json_string(VOC975_CHILD_SEPARATOR));
    json_set(out, "receipt_payload_max_bytes", json_string(VOC975_RECEIPT_MAX));
    json_set(out, "transcript_batch_max", json_string(VOC975_BATCH_MAX));
    json_set(out, "row_annotation_keys", voc975_array_of(VOC975_ROW_ANNOTATION_KEYS, 0));
    json_set(out, "all", voc975_array_of(VOC975_ALL, 0));
    return out;
}

json_t *w20_vocabulary_975e41_row_annotations(const json_t *messages) {
    json_t *out = json_array();
    size_t m, i, count;
    if (!messages || messages->type != JSON_ARRAY) return out;
    count = messages->c.count;
    for (m = 0; m < count; m++) {
        const json_t *message = messages->c.items[m];
        int is_obj = message && message->type == JSON_OBJECT;
        json_t *row = json_object();
        for (i = 0; VOC975_ROW_ANNOTATION_KEYS[i]; i++) {
            const char *key = VOC975_ROW_ANNOTATION_KEYS[i];
            json_t *value = is_obj ? json_obj_get(message, key) : NULL;
            json_set(row, key, value ? json_copy(value) : json_null());
        }
        json_append(out, row);
    }
    return out;
}
