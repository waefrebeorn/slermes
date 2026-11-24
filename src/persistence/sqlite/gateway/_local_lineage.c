/*
 * _local_lineage.c - C11 port of upstream
 * persistence/sqlite/gateway/_local_lineage.py (gate w24_locallinea_6fdf26).
 *
 * PoP: w24_locallinea_6fdf26_moved @ persistence/sqlite/gateway/_local_lineage.py:moved_receipts
 * PoP: w24_locallinea_6fdf26_release_keys @ persistence/sqlite/gateway/_local_lineage.py:release_receipts
 * PoP: w24_locallinea_6fdf26_handoff @ persistence/sqlite/gateway/_local_lineage.py:hand_off_conversations
 * PoP: w24_locallinea_6fdf26_receipt @ persistence/sqlite/gateway/_local_lineage.py:_receipt
 * PoP: w24_locallinea_6fdf26_segment_root @ persistence/sqlite/gateway/_local_lineage.py:_segment_root
 * PoP: w24_locallinea_6fdf26_title_moves @ persistence/sqlite/gateway/_local_lineage.py:_title_follows_head
 * PoP: w24_locallinea_6fdf26_title_rank @ persistence/sqlite/_schema.py:_title_rank
 * PoP: w24_locallinea_6fdf26_constants @ persistence/sqlite/gateway/_local_lineage.py:<module literals>
 *
 * Ported: the routing-database seam is the connection.  Every function here is
 * handed the rows a query would have returned -- the receipt text, the moved
 * (owner, target, source) triples, the segments and their end_reasons, the two
 * title rows -- and decides from them: which receipts moved (narrowed to the
 * rows one repoint rewrote), which keys their release retires, which receipts
 * are handed off (a malformed lineage is refused, so its transcript is not
 * stranded), the segment dedupe that order-preserves the conversation, the head
 * of a /new segment through the compressions behind it, and whether the
 * conversation's name follows that head over a title of lower provenance.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_locallinea_6fdf26.h"

#define LL_POLICY W24_LOCALLINEA_6FDF26_POLICY_PREFIX
#define LL_LEGACY W24_LOCALLINEA_6FDF26_LEGACY_PREFIX

#define LL_LANE_IDENTITY \
    "session_key, source, user_id, chat_id, chat_type, thread_id, display_name, " \
    "origin_json, transport_profile"

/* Python truthiness of a JSON value. */
static int ll_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len((json_t *)v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int ll_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python '==' over the shapes a session id can hold (absent == None). */
static int ll_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    size_t i, n;

    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (ll_number(a, &x) && ll_number(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len((json_t *)a);
        if (n != json_len((json_t *)b)) return 0;
        for (i = 0; i < n; i++)
            if (!ll_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !ll_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

/* ``target in target_ids`` / ``segments.index(target)``. */
static int ll_contains(const json_t *list, const json_t *value) {
    size_t i, n;
    if (!list || list->type != JSON_ARRAY) return 0;
    n = json_len((json_t *)list);
    for (i = 0; i < n; i++)
        if (ll_equal(json_get(list, i), value)) return 1;
    return 0;
}

static int ll_index_of(const json_t *list, const json_t *value, int not_found) {
    size_t i, n;
    if (!list || list->type != JSON_ARRAY) return not_found;
    n = json_len((json_t *)list);
    for (i = 0; i < n; i++)
        if (ll_equal(json_get(list, i), value)) return (int)i;
    return not_found;
}

/* _receipt(raw): json.loads(raw), kept only when it is a dict. */
json_t *w24_locallinea_6fdf26_receipt(const json_t *raw) {
    json_t *parsed;
    if (!raw || raw->type != JSON_STRING || !raw->str_val) return json_null();
    parsed = json_parse(raw->str_val, NULL);      /* TypeError/ValueError -> None */
    if (!parsed) return json_null();
    if (parsed->type != JSON_OBJECT) {
        json_free(parsed);
        return json_null();
    }
    return parsed;
}

json_t *w24_locallinea_6fdf26_moved(const json_t *rows, const json_t *target_ids) {
    json_t *out = json_object();
    size_t i, n;

    if (!rows || rows->type != JSON_ARRAY) return out;
    n = json_len((json_t *)rows);
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        const json_t *owner, *target, *source;
        json_t *pair, *old;
        char keybuf[64];
        const char *key;

        if (!row || row->type != JSON_ARRAY || json_len((json_t *)row) != 3) continue;
        owner = json_get(row, 0);
        target = json_get(row, 1);
        source = json_get(row, 2);
        if (target_ids && target_ids->type == JSON_ARRAY && !ll_contains(target_ids, target))
            continue;                              /* if target_ids is None or target in ... */
        pair = json_array();
        json_append(pair, target ? json_copy(target) : json_null());
        json_append(pair, source ? json_copy(source) : json_null());
        if (owner && owner->type == JSON_STRING && owner->str_val) {
            key = owner->str_val;
        } else {
            /* A non-text owner cannot key a JSON object; its repr is the key. */
            snprintf(keybuf, sizeof(keybuf), "%s", owner ? "<non-string>" : "<null>");
            key = keybuf;
        }
        old = json_obj_get(out, key);
        (void)old;                                 /* dict comprehension: last wins;
                                                    * json_set frees the old value */
        json_set(out, key, pair);
    }
    return out;
}

json_t *w24_locallinea_6fdf26_release_keys(const json_t *owners) {
    json_t *out = json_array();
    size_t i, n;

    if (!owners || owners->type != JSON_ARRAY) return out;
    n = json_len((json_t *)owners);
    for (i = 0; i < n; i++) {
        const json_t *owner = json_get(owners, i);
        json_t *pair;
        char *policy, *legacy;
        size_t need;

        if (!owner || owner->type != JSON_STRING || !owner->str_val) continue;
        need = strlen(LL_POLICY) + strlen(owner->str_val) + 1;
        policy = (char *)malloc(need);
        if (!policy) continue;
        snprintf(policy, need, "%s%s", LL_POLICY, owner->str_val);
        need = strlen(LL_LEGACY) + strlen(owner->str_val) + 1;
        legacy = (char *)malloc(need);
        if (!legacy) { free(policy); continue; }
        snprintf(legacy, need, "%s%s", LL_LEGACY, owner->str_val);
        pair = json_array();
        json_append(pair, json_string(policy));
        json_append(pair, json_string(legacy));
        json_append(out, pair);
        free(policy);
        free(legacy);
    }
    return out;
}

/* ``(_receipt(raw) or {}).get('lineage', [owner])``, validated.  Returns NULL
 * when the lineage is malformed (not a list, or not every element a string),
 * which is the case hand_off_conversations skips. */
static json_t *ll_lineage_of(const json_t *raw, const char *owner, int have_raw) {
    json_t *receipt, *lineage = NULL;
    size_t i, n;

    if (!have_raw) {                               /* saved is None -> [owner] */
        lineage = json_array();
        json_append(lineage, json_string(owner));
        return lineage;
    }
    receipt = w24_locallinea_6fdf26_receipt(raw);  /* dict or null */
    if (receipt->type == JSON_OBJECT) {
        json_t *found = json_obj_get(receipt, "lineage");
        lineage = found ? json_copy(found) : NULL;
    }
    json_free(receipt);
    if (!lineage) {                                /* .get('lineage', [owner]) */
        lineage = json_array();
        json_append(lineage, json_string(owner));
        return lineage;
    }
    if (lineage->type != JSON_ARRAY) {             /* not a list -> refuse */
        json_free(lineage);
        return NULL;
    }
    n = json_len(lineage);
    for (i = 0; i < n; i++) {                      /* all(isinstance(sid, str)) */
        const json_t *sid = json_get(lineage, i);
        if (!sid || sid->type != JSON_STRING) {
            json_free(lineage);
            return NULL;
        }
    }
    return lineage;
}

json_t *w24_locallinea_6fdf26_handoff(const json_t *moved, const json_t *receipts) {
    json_t *handed = json_object();
    json_t *segments_out = json_object();
    json_t *released = json_array();
    json_t *out = json_object();
    size_t i, n;

    if (moved && moved->type == JSON_OBJECT) n = json_object_size(moved);
    else n = 0;

    for (i = 0; i < n; i++) {
        const char *owner = json_object_get_key_at(moved, i);
        const json_t *pair = json_object_get_at(moved, i);
        const json_t *receipt_raw;
        int have_raw;
        json_t *lineage, *segments, *seen;
        size_t j, m;

        if (!owner || !pair || pair->type != JSON_ARRAY || json_len((json_t *)pair) < 2)
            continue;
        have_raw = (receipts && receipts->type == JSON_OBJECT && json_obj_get(receipts, owner))
                       ? 1 : 0;
        receipt_raw = have_raw ? json_obj_get(receipts, owner) : NULL;

        lineage = ll_lineage_of(receipt_raw, owner, have_raw);
        if (!lineage) continue;                    /* malformed: left as it is */

        /* segments = list(dict.fromkeys([owner, *lineage])) */
        segments = json_array();
        seen = json_array();
        json_append(segments, json_string(owner));
        json_append(seen, json_string(owner));
        m = json_len(lineage);
        for (j = 0; j < m; j++) {
            const json_t *sid = json_get(lineage, j);
            if (ll_contains(seen, sid)) continue;
            json_append(seen, json_copy(sid));
            json_append(segments, json_copy(sid));
        }
        json_free(seen);
        json_free(lineage);

        json_set(handed, owner, json_copy(pair));
        json_set(segments_out, owner, segments);
    }

    /* release_receipts(conn, handed) */
    {
        size_t k, hn = json_object_size(handed);
        for (k = 0; k < hn; k++) {
            const char *owner = json_object_get_key_at(handed, k);
            json_t *policy_legacy;
            char *buf;
            size_t need;

            if (!owner) continue;
            need = strlen(LL_POLICY) + strlen(owner) + 1;
            buf = (char *)malloc(need);
            if (!buf) continue;
            snprintf(buf, need, "%s%s", LL_POLICY, owner);
            policy_legacy = json_array();
            json_append(policy_legacy, json_string(buf));
            free(buf);
            need = strlen(LL_LEGACY) + strlen(owner) + 1;
            buf = (char *)malloc(need);
            if (!buf) { json_free(policy_legacy); continue; }
            snprintf(buf, need, "%s%s", LL_LEGACY, owner);
            json_append(policy_legacy, json_string(buf));
            free(buf);
            json_append(released, policy_legacy);
        }
    }

    json_set(out, "handed", handed);
    json_set(out, "segments", segments_out);
    json_set(out, "released", released);
    return out;
}

json_t *w24_locallinea_6fdf26_segment_root(const json_t *segments,
                                           const json_t *target,
                                           const json_t *end_reasons) {
    int index, guard = 0;

    if (!segments || segments->type != JSON_ARRAY) return json_null();
    if (!ll_contains(segments, target))            /* if target not in segments */
        return target ? json_copy(target) : json_null();
    index = ll_index_of(segments, target, -1);
    if (index < 0) return json_null();
    while (index > 0 && guard++ < 4096) {
        /* (row := ...fetchone()) and row[0] == 'compression' */
        const json_t *prev = json_get(segments, index - 1);
        const json_t *reason = NULL;
        if (end_reasons && end_reasons->type == JSON_OBJECT && prev &&
            prev->type == JSON_STRING && prev->str_val)
            reason = json_obj_get(end_reasons, prev->str_val);
        if (!reason || reason->type != JSON_STRING || !reason->str_val ||
            strcmp(reason->str_val, "compression") != 0)
            break;
        index--;
    }
    return json_copy(json_get(segments, index));
}

int w24_locallinea_6fdf26_title_rank(const json_t *source) {
    const char *s;
    if (!source || source->type == JSON_NULL) return 2;   /* NULL ranks as user */
    if (source->type != JSON_STRING || !source->str_val) return 0;  /* str(x): unknown */
    s = source->str_val;
    if (strcmp(s, "user") == 0) return 2;
    if (strcmp(s, "llm") == 0) return 1;
    return 0;                                             /* derived / unknown */
}

int w24_locallinea_6fdf26_title_moves(const json_t *owner, const json_t *head,
                                      const json_t *owner_row,
                                      const json_t *head_row) {
    const json_t *row_title, *row_source, *kept_title, *kept_source;

    if (ll_equal(head, owner)) return 0;                  /* head == owner */
    if (!owner_row || owner_row->type != JSON_ARRAY || json_len((json_t *)owner_row) < 3)
        return 0;                                         /* row is None */
    row_title = json_get(owner_row, 0);
    row_source = json_get(owner_row, 1);
    if (!ll_truthy(row_title)) return 0;                  /* not row[0] */

    if (!head_row || head_row->type != JSON_ARRAY || json_len((json_t *)head_row) < 2)
        return 0;                                         /* kept is None */
    kept_title = json_get(head_row, 0);
    kept_source = json_get(head_row, 1);
    if (ll_truthy(kept_title) &&
        w24_locallinea_6fdf26_title_rank(kept_source) >=
            w24_locallinea_6fdf26_title_rank(row_source))
        return 0;                                         /* a title of >= rank stays */
    return 1;
}

json_t *w24_locallinea_6fdf26_constants(void) {
    json_t *o = json_object();
    json_set(o, "POLICY_PREFIX", json_string(LL_POLICY));
    json_set(o, "LEGACY_PREFIX", json_string(LL_LEGACY));
    json_set(o, "LANE_IDENTITY", json_string(LL_LANE_IDENTITY));
    return o;
}
