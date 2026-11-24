/*
 * prune_rewrite.c - C home for persistence/sqlite/conversation/_store/prune_rewrite.py.
 * Ports the held-row staleness comparator (#124102) and its row/uid naming helpers. The
 * connection work (chunked SELECTs, the in-place UPDATE/twin write) stays Python; this is the
 * decision it makes about which held rows may still be published, on the rows already fetched.
 * Seams mirror the oracle exactly: DB_ROW_SNAPSHOT / the persist marker are the stored column
 * names, message_uid_or_none names a row by its message_uid, transcript_row_snapshot reads the
 * stored-row digest, and _row_identity is the loader-lens identity of role/content/call ids.
 * PoP: w19_prunerewri_17bd63_exact_id @ persistence/sqlite/conversation/_store/prune_rewrite.py:held_row_mismatches
 * PoP: w19_prunerewri_17bd63_call_ids @ persistence/sqlite/conversation/_store/prune_rewrite.py:rewrite_pruned_rows
 * PoP: w19_prunerewri_17bd63_held_row_mismatches @ persistence/sqlite/conversation/_store/prune_rewrite.py:held_row_mismatches
 */
#include "noc_w19_prunerewri_17bd63.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* _DB_PERSISTED_MARKER_KEY / DB_ROW_SNAPSHOT: the stored column names, as the oracle stubs them. */
#define PERSISTED_MARKER "__db_persisted"
#define ROW_SNAPSHOT     "__row_snapshot"
/* The loader-lens identity separator (the oracle's stub joins with U+001F). */
#define IDENTITY_SEP '\x1f'

static char *dupstr(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int obj_truthy(const json_t *v) {
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

static int is_model_role(const char *role) {
    return role && (!strcmp(role, "user") || !strcmp(role, "assistant") || !strcmp(role, "tool"));
}

/* message_uid_or_none: the logical uid when the dict carries one, else NULL. */
static const char *msg_uid(const json_t *message) {
    json_t *uid;
    if (!message || message->type != JSON_OBJECT) return NULL;
    uid = json_obj_get(message, "message_uid");
    return (uid && uid->type == JSON_STRING) ? uid->str_val : NULL;
}

/* int(v) for the scalar values a stored row holds; 0 when int() would raise. */
static int pyint(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        char *end = NULL;
        long long x;
        if (!s || !*s) return 0;
        errno = 0;
        x = strtoll(s, &end, 10);
        if (end == s || *end != '\0') return 0;
        *out = x;
        return 1;
    }
    return 0;
}

long long w19_prunerewri_17bd63_exact_id(json_t *message) {
    json_t *rid;
    if (!message || message->type != JSON_OBJECT) return -1;
    rid = json_obj_get(message, "_row_id");
    if (!rid || rid->type != JSON_NUMBER) return -1;   /* a bool is never an exact id */
    if (rid->num_val <= 0.0) return -1;
    if (rid->num_val != (double)(long long)rid->num_val) return -1;  /* not an int */
    return (long long)rid->num_val;
}

json_t *w19_prunerewri_17bd63_call_ids(json_t *tool_calls) {
    json_t *out = json_array();
    if (tool_calls && tool_calls->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(tool_calls); i++) {
            json_t *call = json_get(tool_calls, i);
            json_t *id = (call && call->type == JSON_OBJECT) ? json_obj_get(call, "id") : NULL;
            json_append(out, id ? json_copy(id) : json_null());
        }
    return out;
}

/* --- loader-lens identity ------------------------------------------------ */

typedef struct { char *p; size_t n, cap; } buf_t;

static void buf_addn(buf_t *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (cap < b->n + n + 1) cap *= 2;
        char *np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void buf_add(buf_t *b, const char *s) { if (s) buf_addn(b, s, strlen(s)); }
static void buf_addc(buf_t *b, char c) { buf_addn(b, &c, 1); }

/* str(value) for the identity parts. */
static void buf_add_str(buf_t *b, const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) return;                    /* "" for None */
    switch (v->type) {
        case JSON_STRING: buf_add(b, v->str_val); break;
        case JSON_BOOL:   buf_add(b, v->bool_val ? "True" : "False"); break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(tmp, sizeof tmp, "%lld", (long long)v->num_val);
            else snprintf(tmp, sizeof tmp, "%g", v->num_val);
            buf_add(b, tmp);
            break;
        default: break;
    }
}

/* _row_identity(role, content, tool_call_id, parsed_calls):
 * "\x1f".join([str(role), content or "", tool_call_id or "", ",".join(call ids)]) */
static char *identity_of(const json_t *role, const json_t *content, const json_t *tool_call_id,
                         const json_t *calls) {
    buf_t b = {NULL, 0, 0};
    buf_add_str(&b, role);
    buf_addc(&b, IDENTITY_SEP);
    buf_add_str(&b, content);
    buf_addc(&b, IDENTITY_SEP);
    buf_add_str(&b, tool_call_id);
    buf_addc(&b, IDENTITY_SEP);
    if (calls && calls->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(calls); i++) {
            json_t *call = json_get(calls, i);
            json_t *id = (call && call->type == JSON_OBJECT) ? json_obj_get(call, "id") : NULL;
            if (i) buf_addc(&b, ',');
            buf_add_str(&b, id);
        }
    }
    if (!b.p) return dupstr("");
    return b.p;
}

/* _parse_tool_calls for the parsed shape a held dict carries. */
static const json_t *parsed_calls(const json_t *v) {
    return (v && v->type == JSON_ARRAY) ? v : NULL;
}

/* --- the comparator ------------------------------------------------------ */

typedef struct { json_t *message; long long row_id; } exact_t;
typedef struct { const char *uid; json_t *message; json_t *row; } uid_slot_t;
typedef struct { long long id; json_t *row; } rowslot_t;

static json_t *row_snapshot(const json_t *row) {
    if (!row || row->type != JSON_OBJECT) return NULL;
    return json_obj_get(row, ROW_SNAPSHOT);
}

static int row_id_of(const json_t *row, long long *out) {
    if (!row || row->type != JSON_OBJECT) return 0;
    return pyint(json_obj_get(row, "id"), out);
}

static int row_matches_uid_query(const json_t *row, const char *session_id, const char *role,
                                 const char *uid) {
    long long active;
    const char *row_role, *row_uid, *row_session;
    if (!row || row->type != JSON_OBJECT) return 0;
    row_session = json_get_str(row, "session_id", NULL);
    if (!row_session || !session_id || strcmp(row_session, session_id)) return 0;
    active = 0;
    pyint(json_obj_get(row, "active"), &active);
    if (!active) return 0;
    row_role = json_get_str(row, "role", NULL);
    row_uid = json_get_str(row, "message_uid", NULL);
    if (!row_role || !role || strcmp(row_role, role)) return 0;
    if (!row_uid || !uid || strcmp(row_uid, uid)) return 0;
    return 1;
}

json_t *w19_prunerewri_17bd63_held_row_mismatches(const char *session_id, json_t *held,
                                                 json_t *stored_rows) {
    json_t *out = json_array();
    size_t held_n = (held && held->type == JSON_ARRAY) ? json_len(held) : 0;
    size_t stored_n = (stored_rows && stored_rows->type == JSON_ARRAY) ? json_len(stored_rows) : 0;
    exact_t *exact = NULL;
    size_t exact_n = 0, by_id_n = 0;
    uid_slot_t *by_uid = NULL;
    size_t by_uid_n = 0;
    rowslot_t *stored = NULL;
    size_t stored_used = 0;

    if (held_n) {
        exact = (exact_t *)calloc(held_n, sizeof *exact);
        by_uid = (uid_slot_t *)calloc(held_n, sizeof *by_uid);
        if (!exact || !by_uid) goto done;
    }
    if (stored_n) {
        stored = (rowslot_t *)calloc(stored_n + held_n + 1, sizeof *stored);
        if (!stored) goto done;
    }

    /* exact = [(m, _exact_id(m)) for m in held if isinstance(m, dict)
     *          and m.get(marker) and m.get("role") in ("user","assistant","tool")] */
    for (size_t i = 0; i < held_n; i++) {
        json_t *m = json_get(held, i);
        if (!m || m->type != JSON_OBJECT) continue;
        if (!obj_truthy(json_obj_get(m, PERSISTED_MARKER))) continue;
        if (!is_model_role(json_get_str(m, "role", NULL))) continue;
        exact[exact_n].message = m;
        exact[exact_n].row_id = w19_prunerewri_17bd63_exact_id(m);
        exact_n++;
    }
    for (size_t i = 0; i < exact_n; i++)
        if (exact[i].row_id >= 0) by_id_n++;

    /* by_uid: the unaddressed replay shape (snapshot + marker, no _row_id). */
    for (size_t i = 0; i < exact_n; i++) {
        json_t *snapshot;
        const char *uid;
        size_t slot;
        if (exact[i].row_id >= 0) continue;
        snapshot = json_obj_get(exact[i].message, ROW_SNAPSHOT);
        if (!snapshot || snapshot->type != JSON_STRING) continue;
        uid = msg_uid(exact[i].message);
        if (!uid) continue;
        slot = by_uid_n;
        for (size_t j = 0; j < by_uid_n; j++)
            if (!strcmp(by_uid[j].uid, uid)) { slot = j; break; }
        if (slot == by_uid_n) by_uid_n++;
        by_uid[slot].uid = uid;
        by_uid[slot].message = exact[i].message;
        by_uid[slot].row = NULL;
    }
    if (!session_id || !session_id[0] || (by_id_n == 0 && by_uid_n == 0)) goto done;

    /* The by-id SELECT: WHERE session_id = ? AND id IN (...) */
    for (size_t i = 0; i < stored_n; i++) {
        json_t *row = json_get(stored_rows, i);
        long long rid;
        const char *row_session;
        int wanted = 0;
        if (!row_id_of(row, &rid)) continue;
        row_session = json_get_str(row, "session_id", NULL);
        if (!row_session || strcmp(row_session, session_id)) continue;
        for (size_t j = 0; j < exact_n; j++)
            if (exact[j].row_id == rid) { wanted = 1; break; }
        if (!wanted) continue;
        stored[stored_used].id = rid;
        stored[stored_used].row = row;
        stored_used++;
    }
    /* The by-uid SELECT: the newest active row of that role with that message_uid. */
    for (size_t u = 0; u < by_uid_n; u++) {
        json_t *best = NULL;
        long long best_id = 0;
        const char *role = json_get_str(by_uid[u].message, "role", NULL);
        for (size_t i = 0; i < stored_n; i++) {
            json_t *row = json_get(stored_rows, i);
            long long rid;
            if (!row_matches_uid_query(row, session_id, role, by_uid[u].uid)) continue;
            if (!row_id_of(row, &rid)) continue;
            if (!best || rid > best_id) { best = row; best_id = rid; }
        }
        if (!best) continue;
        by_uid[u].row = best;
        stored[stored_used].id = best_id;
        stored[stored_used].row = best;
        stored_used++;
    }

    for (size_t i = 0; i < exact_n; i++) {
        json_t *message = exact[i].message;
        json_t *row = NULL;
        long long active = 0, rid = 0;
        char *held_identity, *stored_identity;
        json_t *snapshot;

        if (exact[i].row_id >= 0) {
            for (size_t j = 0; j < stored_used; j++)
                if (stored[j].id == exact[i].row_id) { row = stored[j].row; break; }
        } else {
            const char *uid = msg_uid(message);
            if (uid)
                for (size_t j = 0; j < by_uid_n; j++)
                    if (!strcmp(by_uid[j].uid, uid)) { row = by_uid[j].row; break; }
        }
        if (!row) continue;
        pyint(json_obj_get(row, "active"), &active);
        row_id_of(row, &rid);
        if (!active) continue;                       /* the activity check owns absent rows */
        snapshot = json_obj_get(message, ROW_SNAPSHOT);
        if (snapshot && snapshot->type == JSON_STRING) {
            json_t *stored_snap = row_snapshot(row);
            const char *a = (stored_snap && stored_snap->type == JSON_STRING)
                            ? stored_snap->str_val : NULL;
            if (!a || strcmp(a, snapshot->str_val)) json_append(out, json_number((double)rid));
            continue;
        }
        held_identity = identity_of(json_obj_get(message, "role"),
                                    json_obj_get(message, "content"),
                                    json_obj_get(message, "tool_call_id"),
                                    parsed_calls(json_obj_get(message, "tool_calls")));
        stored_identity = identity_of(json_obj_get(row, "role"),
                                      json_obj_get(row, "content"),
                                      json_obj_get(row, "tool_call_id"),
                                      parsed_calls(json_obj_get(row, "tool_calls")));
        if (!held_identity || !stored_identity || strcmp(held_identity, stored_identity))
            json_append(out, json_number((double)rid));
        free(held_identity);
        free(stored_identity);
    }

done:
    free(exact);
    free(by_uid);
    free(stored);
    return out;
}
