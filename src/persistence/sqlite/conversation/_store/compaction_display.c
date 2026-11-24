/*
 * compaction_display.c -- real C11 port of
 * persistence/sqlite/conversation/_store/compaction_display.py (gate
 * w21_compaction_8a06a1): the filter and its Python twin that make a compacted
 * tool result paint once, as its archived original.
 *
 * PoP: w21_compaction_8a06a1_not_a_later_tool_copy_sql @ persistence/sqlite/conversation/_store/compaction_display.py:not_a_later_tool_copy_sql
 * PoP: w21_compaction_8a06a1_later_tool_copy_ids @ persistence/sqlite/conversation/_store/compaction_display.py:later_tool_copy_ids
 *
 * The pieces the module builds on are ported too, because none of them reaches
 * a database: `_sql_json_extract` from persistence/sqlite/_schema_ddl.py,
 * `MODEL_ONLY_DISPLAY_METADATA_KEY` from session_rules/turns.py, and the
 * display-metadata decode plus `_is_model_only_row` from the store's
 * messages module (a pure reader of the row's `display_metadata` column).
 */
#include "hermes_json.h"
#include "noc_w21_compaction_8a06a1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* session_rules/turns.py:MODEL_ONLY_DISPLAY_METADATA_KEY */
static const char *const CMP806_MODEL_ONLY_KEY = "model_only";

static char *cmp806_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* `_sql_literal(text)`: "'" + text.replace("'", "''") + "'". Caller appends. */
static size_t cmp806_sql_literal(char *out, size_t cap, const char *text) {
    size_t used = 0;
    if (cap < 3) return 0;
    out[used++] = '\'';
    for (; text && *text && used + 3 < cap; text++) {
        if (*text == '\'') out[used++] = '\'';
        out[used++] = *text;
    }
    out[used++] = '\'';
    out[used] = '\0';
    return used;
}

/* `_sql_json_extract(expression, path)`: a non-throwing JSON marker lookup for
 * a JSON TEXT column. Caller appends to `out`. */
static void cmp806_json_extract(char *out, size_t cap, const char *expression, const char *path) {
    char literal[512];
    cmp806_sql_literal(literal, sizeof(literal), path);
    snprintf(out, cap,
             "json_extract((CASE WHEN json_valid(%s) THEN %s ELSE json_object() END), %s)",
             expression, expression, literal);
}

char *w21_compaction_8a06a1_not_a_later_tool_copy_sql(const char *alias) {
    const char *a = alias ? alias : "";
    char visible[1024];
    char *out;
    size_t cap = strlen(a) * 8 + 2048;
    char path[64];
    snprintf(path, sizeof(path), "$.%s", CMP806_MODEL_ONLY_KEY);
    cmp806_json_extract(visible, sizeof(visible), "original.display_metadata", path);
    out = (char *)malloc(cap);
    if (!out) return NULL;
    snprintf(out, cap,
             " AND NOT (%s.role = 'tool' AND %s.message_uid IS NOT NULL AND EXISTS ("
             "SELECT 1 FROM messages AS original WHERE original.session_id = %s.session_id "
             "AND original.timestamp = %s.timestamp AND original.message_uid = %s.message_uid "
             "AND original.role = 'tool' AND original.active = 0 AND original.compacted = 1 "
             "AND +original.display_order < %s.display_order AND COALESCE(%s, 0) = 0))",
             a, a, a, a, a, a, visible);
    return out;
}

/* Python truthiness for a decoded JSON value; NULL (absent) reads as None. */
static int cmp806_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* `_json_or(raw, _INVALID, warning)`: json.loads, falling back on invalid text. */
static json_t *cmp806_json_or(const json_t *raw) {
    if (!raw || raw->type != JSON_STRING || !raw->str_val) return NULL;
    return json_parse(raw->str_val, NULL);
}

/* `_decode_display_metadata(raw)`: two string layers unwrapped, a non-object
 * or an unparsable value reading as None. */
static json_t *cmp806_decode_meta(const json_t *raw) {
    json_t *meta = NULL, *tmp;
    int layer;
    if (!raw) return NULL;
    if (raw->type == JSON_STRING) {
        meta = cmp806_json_or(raw);
        for (layer = 0; layer < 1 && meta && meta->type == JSON_STRING; layer++) {
            tmp = cmp806_json_or(meta);
            json_free(meta);
            meta = tmp;
        }
    } else if (raw->type == JSON_OBJECT) {
        meta = json_copy(raw);
    } else {
        return NULL;
    }
    if (!meta) return NULL;
    if (meta->type != JSON_OBJECT) {
        json_free(meta);
        return NULL;
    }
    return meta;
}

/* `_is_model_only_row(holder, row)`: display_metadata's model-only marker. */
static int cmp806_is_model_only(const json_t *row) {
    json_t *meta, *marker;
    int answer;
    if (!row || row->type != JSON_OBJECT) return 0;
    meta = cmp806_decode_meta(json_obj_get(row, "display_metadata"));
    if (!meta) return 0;
    marker = json_obj_get(meta, CMP806_MODEL_ONLY_KEY);
    answer = cmp806_truthy(marker);
    json_free(meta);
    return answer;
}

/* A row field as text, so an owner tuple compares without a type zoo: the same
 * JSON value spells the same text on both sides. NULL when absent. */
static const char *cmp806_text(const json_t *row, const char *key, char *buf, size_t cap) {
    json_t *v = json_obj_get(row, key);
    if (!v) return NULL;
    switch (v->type) {
    case JSON_STRING:
        return v->str_val ? v->str_val : "";
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(char) * cap, "%lld", (long long)v->num_val);
        else
            snprintf(buf, cap, "%.17g", v->num_val);
        return buf;
    case JSON_BOOL:
        return v->bool_val ? "true" : "false";
    case JSON_NULL:
        return NULL;
    default:
        return NULL;
    }
}

static int cmp806_slot(const json_t *row, long long *slot) {
    json_t *v = json_obj_get(row, "display_order");
    if (!v || v->type == JSON_NULL) return 0;   /* `slot is None` */
    if (v->type != JSON_NUMBER) return 0;
    *slot = (long long)v->num_val;
    return 1;
}

json_t *w21_compaction_8a06a1_later_tool_copy_ids(const json_t *rows) {
    json_t *out = json_array();
    size_t n, i, j;
    const json_t **cands = NULL;
    long long *row_slot = NULL;
    int *has_slot = NULL;
    char (*keys)[3][512] = NULL;
    size_t nc = 0;
    int tools_seen = 0;
    struct owner { char key[1536]; long long slot; } *owners = NULL;
    size_t no = 0;
    long long *out_ids = NULL;
    size_t out_n = 0;

    if (!rows || rows->type != JSON_ARRAY) return out;
    n = rows->c.count;
    if (n == 0) return out;

    cands = (const json_t **)calloc(n, sizeof(*cands));
    row_slot = (long long *)calloc(n, sizeof(*row_slot));
    has_slot = (int *)calloc(n, sizeof(*has_slot));
    keys = (char (*)[3][512])calloc(n, sizeof(*keys));
    owners = (struct owner *)calloc(n ? n : 1, sizeof(*owners));
    out_ids = (long long *)calloc(n ? n : 1, sizeof(*out_ids));
    if (!cands || !row_slot || !has_slot || !keys || !owners || !out_ids) goto done;

    /* candidates = [row for row in tools if row["active"] or row["compacted"]] */
    for (i = 0; i < n; i++) {
        json_t *row = rows->c.items[i];
        json_t *role;
        if (!row || row->type != JSON_OBJECT) continue;
        role = json_obj_get(row, "role");
        if (!role || role->type != JSON_STRING || strcmp(role->str_val ? role->str_val : "", "tool")) continue;
        if (!nc && !tools_seen) {                 /* tools[0].keys() */
            tools_seen = 1;
            if (!json_obj_get(row, "compacted")) return out;
        }
        if (!cmp806_truthy(json_obj_get(row, "active")) && !cmp806_truthy(json_obj_get(row, "compacted"))) continue;
        if (!nc && !json_obj_get(row, "display_order")) return out;  /* candidates[0].keys() */
        cands[nc] = row;
        if (cmp806_slot(row, &row_slot[nc])) has_slot[nc] = 1;
        nc++;
    }
    if (nc == 0) goto done;

    for (i = 0; i < nc; i++) {
        char buf[512];
        int k;
        for (k = 0; k < 3; k++) {
            static const char *const OWNER_KEYS[3] = {"session_id", "message_uid", "timestamp"};
            const char *t = cmp806_text(cands[i], OWNER_KEYS[k], buf, sizeof(buf));
            snprintf(keys[i][k], 512, "%s", t ? t : "");
        }
    }
    /* first_slot[owner] keeps the lowest slot any visible archived row had */
    for (i = 0; i < nc; i++) {
        char owner_key[1536];
        long long slot = row_slot[i];
        if (!cmp806_truthy(json_obj_get(cands[i], "message_uid"))) continue;
        if (!has_slot[i]) continue;
        if (cmp806_truthy(json_obj_get(cands[i], "active"))) continue;
        if (cmp806_is_model_only(cands[i])) continue;
        snprintf(owner_key, sizeof(owner_key), "%s\x1f%s\x1f%s", keys[i][0], keys[i][1], keys[i][2]);
        for (j = 0; j < no; j++) {
            if (!strcmp(owners[j].key, owner_key)) break;
        }
        if (j == no) {
            snprintf(owners[no].key, sizeof(owners[no].key), "%s", owner_key);
            owners[no].slot = slot;
            no++;
        } else if (slot < owners[j].slot) {
            owners[j].slot = slot;
        }
    }
    /* A row is a later copy of an archived original when its owner already had
     * an earlier visible slot. */
    for (i = 0; i < nc; i++) {
        char owner_key[1536];
        long long slot = row_slot[i], base;
        json_t *idv;
        if (!cmp806_truthy(json_obj_get(cands[i], "message_uid"))) continue;
        if (!has_slot[i]) continue;
        snprintf(owner_key, sizeof(owner_key), "%s\x1f%s\x1f%s", keys[i][0], keys[i][1], keys[i][2]);
        base = slot;                                  /* first_slot.get(owner, display_order) */
        for (j = 0; j < no; j++) {
            if (!strcmp(owners[j].key, owner_key)) { base = owners[j].slot; break; }
        }
        if (!(base < slot)) continue;
        idv = json_obj_get(cands[i], "id");
        out_ids[out_n++] = idv && idv->type == JSON_NUMBER ? (long long)idv->num_val : 0;
    }
    /* a set of ids: sorted ascending so the wire form is stable */
    for (i = 0; i + 1 < out_n; i++) {
        for (j = i + 1; j < out_n; j++) {
            if (out_ids[j] < out_ids[i]) {
                long long t = out_ids[i];
                out_ids[i] = out_ids[j];
                out_ids[j] = t;
            }
        }
    }
    for (i = 0; i < out_n; i++) {
        if (i && out_ids[i] == out_ids[i - 1]) continue;
        json_append(out, json_number((double)out_ids[i]));
    }

done:
    free(cands); free(row_slot); free(has_slot); free(keys); free(owners); free(out_ids);
    return out;
}
