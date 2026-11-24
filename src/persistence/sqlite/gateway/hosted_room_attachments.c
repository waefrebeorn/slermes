/*
 * hosted_room_attachments.c - C11 port of upstream
 * persistence/sqlite/gateway/hosted_room_attachments.py
 * (gates w30_hostedroom_fe22db_p1 / w30_hostedroom_fe22db_p2).
 *
 * PORTED (pure value -> value; one PoP line per symbol):
 *   PoP: w30_hostedroom_fe22db_p1_metadata @ persistence/sqlite/gateway/hosted_room_attachments.py:_metadata
 *   PoP: w30_hostedroom_fe22db_p1_blob_id_ok @ persistence/sqlite/gateway/hosted_room_attachments.py:_BLOB_ID_RE
 *   PoP: w30_hostedroom_fe22db_p1_blob_path @ persistence/sqlite/gateway/hosted_room_attachments.py:_blob_path
 *   PoP: w30_hostedroom_fe22db_p1_owner_event @ persistence/sqlite/gateway/hosted_room_attachments.py:_owner_event
 *   PoP: w30_hostedroom_fe22db_p1_named_by_owner_event @ persistence/sqlite/gateway/hosted_room_attachments.py:_named_by_owner_event
 *   PoP: w30_hostedroom_fe22db_p1_sweep_select @ persistence/sqlite/gateway/hosted_room_attachments.py:_sweep_orphans
 *   PoP: w30_hostedroom_fe22db_p1_prune_plan @ persistence/sqlite/gateway/hosted_room_attachments.py:_prune
 *   PoP: w30_hostedroom_fe22db_p2_constants @ session_rules/hosted_attachments.py:<limits> + hosted_room_attachments.py:_CURRENT_COLUMNS
 *   PoP: w30_hostedroom_fe22db_p2_manifest_validate @ session_rules/hosted_attachments.py:validate_manifest
 *   PoP: w30_hostedroom_fe22db_p2_retain @ persistence/sqlite/gateway/hosted_room_attachments.py:retain_message_attachments
 *   PoP: w30_hostedroom_fe22db_p2_require_viewer_room @ persistence/sqlite/gateway/hosted_room_attachments.py:_require_viewer_room
 *   PoP: w30_hostedroom_fe22db_p2_read_gate @ persistence/sqlite/gateway/hosted_room_attachments.py:_read_committed_row
 *   PoP: w30_hostedroom_fe22db_p2_range_slice @ persistence/sqlite/gateway/hosted_room_attachments.py:_read_blob_range
 *
 * SEAMED (fed as fixture inputs, never ported here): the sqlite3 connection and
 * the statements it runs (the fixture's ``db`` snapshot stands in for the two
 * files' tables), the blob directory (open/fstat/read/iterdir/replace/unlink),
 * the store handle (BEGIN IMMEDIATE, commit/rollback, read-only handles), the
 * clock, secrets.token_hex, the file identity and the blob SHA-256.
 *
 * Number note: JSON has one number type, so an integral float (5.0) is read as
 * the int the Python parser would have produced; upstream would refuse it as a
 * non-int ``size``.  The fixtures use a non-integral float for that refusal.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_hostedroom_fe22db_p1.h"
#include "noc_w30_hostedroom_fe22db_p2.h"

/* ── Python value semantics ──────────────────────────────────────────────── */

static int w30_truthy(const json_t *v) {
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

static int w30_num(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python str() over the scalars a row or payload element can hold. */
static char *w30_str_of(const json_t *v) {
    char buf[64];
    if (!v) return strdup("None");
    switch (v->type) {
    case JSON_STRING:
        return strdup(v->str_val ? v->str_val : "");
    case JSON_BOOL:
        return strdup(v->bool_val ? "True" : "False");
    case JSON_NULL:
        return strdup("None");
    case JSON_NUMBER: {
        double n = v->num_val;
        if (n >= -9.2233720368547758e18 && n <= 9.2233720368547758e18 &&
            n == (double)(long long)n)
            snprintf(buf, sizeof buf, "%lld", (long long)n);
        else
            snprintf(buf, sizeof buf, "%.17g", n);
        return strdup(buf);
    }
    case JSON_ARRAY:  return strdup("[]");
    case JSON_OBJECT: return strdup("{}");
    }
    return strdup("");
}

/* ``str(x or "")``: falsy values read as the empty string. */
static char *w30_str_or_empty(const json_t *v) {
    return w30_truthy(v) ? w30_str_of(v) : strdup("");
}

/* Python int(): bool, a truncating number, or a base-10 string. */
static int w30_int_of(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        long long got;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
        got = strtoll(s, &end, 10);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' ||
               *end == '\f' || *end == '\v') end++;
        if (*end != '\0') return 0;
        *out = got;
        return 1;
    }
    return 0;
}

/* Python float(): bool, a number, or a float-shaped string. */
static int w30_float_of(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        double got;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
        if (*s == '\0') return 0;
        got = strtod(s, &end);
        if (end == s) return 0;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' ||
               *end == '\f' || *end == '\v') end++;
        if (*end != '\0') return 0;
        *out = got;
        return 1;
    }
    return 0;
}

/* Python == over the shapes a row value can hold; absent and null are equal. */
static int w30_eq(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (w30_num(a, &x) && w30_num(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!w30_eq(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !w30_eq(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

static const json_t *w30_get(const json_t *o, const char *key) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, key) : NULL;
}

/* A statement parameter: a null/missing value never matches a row (SQL NULL). */
static int w30_param_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

/* ── answers ─────────────────────────────────────────────────────────────── */

static json_t *w30_ok(json_t *value) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(1));
    json_set(o, "value", value ? value : json_null());
    return o;
}

static json_t *w30_err_json(const char *type, const char *message) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(0));
    json_set(o, "type", json_string(type ? type : "Exception"));
    json_set(o, "message", message ? json_string(message) : json_null());
    return o;
}

static int w30_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *w30_string_array(const char *const *items, size_t n) {
    json_t *arr = json_array();
    size_t i;
    for (i = 0; i < n; i++) json_append(arr, json_string(items[i]));
    return arr;
}

/* ── the db snapshot ─────────────────────────────────────────────────────── */

typedef struct {
    const json_t *rooms;        /* NULL: table absent */
    const json_t *events;       /* NULL: table absent */
    const json_t *quarantine;   /* NULL: table absent */
    const json_t *fences;       /* NULL: table absent */
    const json_t *attachments;  /* rows; has_attachments says whether the table exists */
    const json_t *blobs;
    const json_t *files;        /* names in the blob root */
    int has_attachments;
    int has_blobs;
} w30_db;

static int w30_dropped(const json_t *db, const char *table) {
    const json_t *drop = w30_get(db, "drop");
    size_t i, n;
    if (!drop || drop->type != JSON_ARRAY) return 0;
    n = json_len(drop);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(drop, i);
        if (v && v->type == JSON_STRING && strcmp(v->str_val ? v->str_val : "", table) == 0)
            return 1;
    }
    return 0;
}

static void w30_db_load(w30_db *d, const json_t *db) {
    memset(d, 0, sizeof *d);
    d->has_attachments = !w30_dropped(db, "hosted_room_attachments");
    d->has_blobs = !w30_dropped(db, "hosted_room_attachment_blobs");
    d->attachments = d->has_attachments ? w30_get(db, "attachments") : NULL;
    d->blobs = d->has_blobs ? w30_get(db, "blobs") : NULL;
    if (d->attachments && d->attachments->type != JSON_ARRAY) d->attachments = NULL;
    if (d->blobs && d->blobs->type != JSON_ARRAY) d->blobs = NULL;
    if (!w30_dropped(db, "hosted_rooms")) d->rooms = w30_get(db, "rooms");
    if (!w30_dropped(db, "hosted_room_events")) d->events = w30_get(db, "events");
    if (!w30_dropped(db, "hosted_room_quarantine")) d->quarantine = w30_get(db, "quarantine");
    if (!w30_dropped(db, "hosted_room_disband_fences")) d->fences = w30_get(db, "fences");
    d->files = w30_get(db, "files");
    if (d->files && d->files->type != JSON_ARRAY) d->files = NULL;
}

static const json_t *w30_first(const json_t *arr, const char *key, const json_t *value) {
    size_t i, n = json_len(arr);
    if (!arr || arr->type != JSON_ARRAY || w30_param_null(value)) return NULL;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(arr, i);
        if (!row || row->type != JSON_OBJECT) continue;
        if (w30_eq(json_obj_get(row, key), value)) return row;
    }
    return NULL;
}

static const json_t *w30_first2(const json_t *arr, const char *k1, const json_t *v1,
                                const char *k2, const json_t *v2) {
    size_t i, n = json_len(arr);
    if (!arr || arr->type != JSON_ARRAY || w30_param_null(v1) || w30_param_null(v2)) return NULL;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(arr, i);
        if (!row || row->type != JSON_OBJECT) continue;
        if (w30_eq(json_obj_get(row, k1), v1) && w30_eq(json_obj_get(row, k2), v2)) return row;
    }
    return NULL;
}

/* The same lookup with the two parameters already stringified (SQL binds TEXT). */
static const json_t *w30_first2_str(const json_t *arr, const char *k1, const char *v1,
                                    const char *k2, const char *v2) {
    size_t i, n = json_len(arr);
    if (!arr || arr->type != JSON_ARRAY || !v1 || !v2) return NULL;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(arr, i);
        const json_t *a, *b;
        if (!row || row->type != JSON_OBJECT) continue;
        a = json_obj_get(row, k1);
        b = json_obj_get(row, k2);
        if (!a || a->type != JSON_STRING || !b || b->type != JSON_STRING) continue;
        if (strcmp(a->str_val ? a->str_val : "", v1) == 0 &&
            strcmp(b->str_val ? b->str_val : "", v2) == 0) return row;
    }
    return NULL;
}

/* The same lookup with the key already stringified (SQL binds TEXT). */
static const json_t *w30_first_str(const json_t *arr, const char *key, const char *value) {
    size_t i, n = json_len(arr);
    if (!arr || arr->type != JSON_ARRAY || !value) return NULL;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(arr, i);
        const json_t *v;
        if (!row || row->type != JSON_OBJECT) continue;
        v = json_obj_get(row, key);
        if (v && v->type == JSON_STRING && strcmp(v->str_val ? v->str_val : "", value) == 0)
            return row;
    }
    return NULL;
}

/* The owner event of a published attachment: kind IN ('message.user','message.member'). */
static const json_t *w30_event_owner(const json_t *events, const char *room_id,
                                     const char *event_id) {
    size_t i, n = json_len(events);
    if (!events || events->type != JSON_ARRAY || !room_id || !event_id) return NULL;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(events, i);
        const json_t *r, *e, *k;
        if (!row || row->type != JSON_OBJECT) continue;
        r = json_obj_get(row, "room_id");
        e = json_obj_get(row, "event_id");
        k = json_obj_get(row, "kind");
        if (!r || r->type != JSON_STRING || !e || e->type != JSON_STRING) continue;
        if (strcmp(r->str_val ? r->str_val : "", room_id) != 0) continue;
        if (strcmp(e->str_val ? e->str_val : "", event_id) != 0) continue;
        if (!k || k->type != JSON_STRING) continue;
        if (strcmp(k->str_val ? k->str_val : "", "message.user") != 0 &&
            strcmp(k->str_val ? k->str_val : "", "message.member") != 0) continue;
        return row;
    }
    return NULL;
}

/* ── blob ids ────────────────────────────────────────────────────────────── */

static int w30_hex32(const char *s) {
    size_t i;
    for (i = 0; i < 32; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return s[32] == '\0';
}

static int w30_blob_id_ok_str(const char *s) {
    return s && strncmp(s, "blob_", 5) == 0 && w30_hex32(s + 5);
}

static int w30_attachment_id_ok_str(const char *s) {
    return s && strncmp(s, "att_", 4) == 0 && w30_hex32(s + 4);
}

/* _BLOB_ID_RE.fullmatch(value) is not None; a non-string is a TypeError. */
json_t *w30_hostedroom_fe22db_p1_blob_id_ok(const json_t *in) {
    const json_t *v = w30_get(in, "value");
    if (!v || v->type != JSON_STRING) return w30_err_json("TypeError", NULL);
    return w30_ok(json_bool(w30_blob_id_ok_str(v->str_val ? v->str_val : "")));
}

json_t *w30_hostedroom_fe22db_p1_blob_path(const json_t *in) {
    const json_t *v = w30_get(in, "blob_id");
    if (!v || v->type != JSON_STRING) return w30_err_json("TypeError", NULL);
    if (!w30_blob_id_ok_str(v->str_val ? v->str_val : ""))
        return w30_err_json("AttachmentIntegrityError", "stored blob id is invalid");
    return w30_ok(json_string(v->str_val ? v->str_val : ""));
}

/* ── the attachment envelope ─────────────────────────────────────────────── */

/* _metadata(row, idempotent=...): every field is read, str()/int()/float()d. */
static json_t *w30_envelope(const json_t *row, int idempotent, const char **etype) {
    static const char *const str_keys[6] = {
        "attachment_id", "kind", "name", "mime", "sha256", "state",
    };
    const json_t *size, *created, *event;
    long long size_value;
    double created_value;
    json_t *out;
    int i;

    if (!row || row->type != JSON_OBJECT) { *etype = "TypeError"; return NULL; }
    for (i = 0; i < 6; i++)
        if (!json_obj_get(row, str_keys[i])) { *etype = "KeyError"; return NULL; }
    size = json_obj_get(row, "size");
    created = json_obj_get(row, "created_at");
    event = json_obj_get(row, "event_id");
    if (!size || !created || !event) { *etype = "KeyError"; return NULL; }
    if (!w30_int_of(size, &size_value)) { *etype = "ValueError"; return NULL; }
    if (!w30_float_of(created, &created_value)) { *etype = "ValueError"; return NULL; }

    out = json_object();
    json_set(out, "attachment_id", json_string(w30_str_of(json_obj_get(row, "attachment_id"))));
    json_set(out, "kind", json_string(w30_str_of(json_obj_get(row, "kind"))));
    json_set(out, "name", json_string(w30_str_of(json_obj_get(row, "name"))));
    json_set(out, "size", json_number((double)size_value));
    json_set(out, "mime", json_string(w30_str_of(json_obj_get(row, "mime"))));
    json_set(out, "sha256", json_string(w30_str_of(json_obj_get(row, "sha256"))));
    json_set(out, "state", json_string(w30_str_of(json_obj_get(row, "state"))));
    json_set(out, "created_at", json_number(created_value));
    json_set(out, "idempotent", json_bool(idempotent));
    if (event->type != JSON_NULL)
        json_set(out, "event_id", json_string(w30_str_of(event)));
    return out;
}

json_t *w30_hostedroom_fe22db_p1_metadata(const json_t *in) {
    const char *etype = NULL;
    json_t *env = w30_envelope(w30_get(in, "row"), w30_truthy(w30_get(in, "idempotent")), &etype);
    if (!env) return w30_err_json(etype, NULL);
    return w30_ok(env);
}

/* ── the durable event that owns an attachment ───────────────────────────── */

json_t *w30_hostedroom_fe22db_p1_owner_event(const json_t *in) {
    w30_db d;
    const json_t *row;
    json_t *out;
    w30_db_load(&d, w30_get(in, "db"));
    if (!d.events || d.events->type != JSON_ARRAY) return w30_ok(json_null());
    row = w30_first2(d.events, "room_id", w30_get(in, "room_id"),
                     "event_id", w30_get(in, "event_id"));
    if (!row) return w30_ok(json_null());
    out = json_object();
    {
        const json_t *payload = json_obj_get(row, "payload_json");
        json_set(out, "payload_json", payload ? json_copy(payload) : json_null());
    }
    return w30_ok(out);
}

/* The reconciliation's rule for one row: its durable event payload names it. */
static int w30_named_in_payload(const char *payload_json, const char *attachment_id) {
    json_t *payload;
    char *err = NULL;
    const json_t *attachments;
    size_t i, n;
    int found = 0;

    if (!payload_json) return 0;
    payload = json_parse(payload_json, &err);
    if (err) free(err);
    if (!payload) return 0;
    if (payload->type != JSON_OBJECT) { json_free(payload); return 0; }
    attachments = json_obj_get(payload, "attachments");
    if (!attachments) { json_free(payload); return 0; }
    switch (attachments->type) {
    case JSON_ARRAY:
        n = json_len(attachments);
        for (i = 0; i < n && !found; i++) {
            const json_t *item = json_get(attachments, i);
            char *cand;
            if (!item || item->type != JSON_OBJECT) continue;
            cand = w30_str_or_empty(json_obj_get(item, "attachment_id"));
            if (strcmp(cand, attachment_id) == 0) found = 1;
            free(cand);
        }
        break;
    case JSON_OBJECT:
    case JSON_STRING:
        break;                 /* keys/characters are not Mapping entries */
    default:
        json_free(payload);
        return 0;              /* null is not iterable, a number is not too */
    }
    json_free(payload);
    return found;
}

static int w30_named_by_owner(const w30_db *d, const json_t *row, const char **etype) {
    const json_t *room_id, *event_id, *attachment_id;
    const json_t *owner;
    char *room, *event, *att, *payload;
    int named = 0;

    if (!row || row->type != JSON_OBJECT) { *etype = "TypeError"; return 0; }
    room_id = json_obj_get(row, "room_id");
    event_id = json_obj_get(row, "event_id");
    attachment_id = json_obj_get(row, "attachment_id");
    if (!room_id || !event_id || !attachment_id) { *etype = "KeyError"; return 0; }
    room = w30_str_of(room_id);
    event = w30_str_or_empty(event_id);
    att = w30_str_of(attachment_id);

    if (d->events && d->events->type == JSON_ARRAY) {
        owner = w30_first2_str(d->events, "room_id", room, "event_id", event);
        if (owner) {
            payload = w30_str_of(json_obj_get(owner, "payload_json"));
            named = w30_named_in_payload(payload, att);
            free(payload);
        }
    }
    free(room);
    free(event);
    free(att);
    return named;
}

json_t *w30_hostedroom_fe22db_p1_named_by_owner_event(const json_t *in) {
    w30_db d;
    const char *etype = NULL;
    int named;
    w30_db_load(&d, w30_get(in, "db"));
    named = w30_named_by_owner(&d, w30_get(in, "row"), &etype);
    if (etype) return w30_err_json(etype, NULL);
    return w30_ok(json_bool(named));
}

/* ── the orphan sweep's selection ────────────────────────────────────────── */

static int w30_blob_live(const w30_db *d, const char *name) {
    size_t j, m;
    if (!d->blobs || d->blobs->type != JSON_ARRAY) return 0;
    m = json_len(d->blobs);
    for (j = 0; j < m; j++) {
        char *id = w30_str_of(w30_get(json_get(d->blobs, j), "blob_id"));
        int same = strcmp(id, name) == 0;
        free(id);
        if (same) return 1;
    }
    return 0;
}

static int w30_orphan(const w30_db *d, const char *name) {
    return strncmp(name, ".tmp-", 5) == 0 || strncmp(name, ".prune-", 7) == 0 ||
           (w30_blob_id_ok_str(name) && !w30_blob_live(d, name));
}

json_t *w30_hostedroom_fe22db_p1_sweep_select(const json_t *in) {
    w30_db d;
    const json_t *names;
    const char **removed, **remaining;
    size_t i, n, nr = 0, nk = 0;
    json_t *out;

    w30_db_load(&d, w30_get(in, "db"));
    names = d.files;
    n = names ? json_len(names) : 0;
    removed = (const char **)calloc(n ? n : 1, sizeof(char *));
    remaining = (const char **)calloc(n ? n : 1, sizeof(char *));
    if (!removed || !remaining) {
        free(removed);
        free(remaining);
        return w30_ok(json_null());
    }
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(names, i);
        const char *name;
        if (!v || v->type != JSON_STRING) continue;
        name = v->str_val ? v->str_val : "";
        if (w30_orphan(&d, name)) removed[nr++] = name; else remaining[nk++] = name;
    }
    qsort(removed, nr, sizeof(char *), w30_cmp_str);
    qsort(remaining, nk, sizeof(char *), w30_cmp_str);
    out = json_object();
    json_set(out, "removed", w30_string_array(removed, nr));
    json_set(out, "remaining", w30_string_array(remaining, nk));
    free(removed);
    free(remaining);
    return w30_ok(out);
}

/* ── reclamation: disband, expiry, blob reference counts, sweep ──────────── */

typedef struct {
    const json_t *room_id;
    char *id;
    char *blob;
    char *state;
    int has_expiry;
    double expiry;
    int dead;
} w30_att;

typedef struct {
    char *id;
    double refs;
    int dead;
} w30_blob;

json_t *w30_hostedroom_fe22db_p1_prune_plan(const json_t *in) {
    w30_db d;
    w30_att *atts = NULL;
    w30_blob *blobs = NULL;
    size_t na = 0, nb = 0, i, j;
    double now = 0.0;
    int removed = 0;
    json_t *out = NULL;
    const char **deleted = NULL, **files = NULL, **seen = NULL;
    size_t ndel = 0, nfiles = 0, nseen = 0;

    w30_db_load(&d, w30_get(in, "db"));
    w30_num(w30_get(in, "now"), &now);

    if (d.attachments) na = json_len(d.attachments);
    if (d.blobs) nb = json_len(d.blobs);
    atts = (w30_att *)calloc(na ? na : 1, sizeof *atts);
    blobs = (w30_blob *)calloc(nb ? nb : 1, sizeof *blobs);
    deleted = (const char **)calloc(na ? na : 1, sizeof(char *));
    seen = (const char **)calloc(na ? na : 1, sizeof(char *));
    files = (const char **)calloc((d.files ? json_len(d.files) : 0) + 1, sizeof(char *));
    if (!atts || !blobs || !deleted || !seen || !files) goto done;

    for (i = 0; i < na; i++) {
        const json_t *row = json_get(d.attachments, i);
        const json_t *expiry;
        if (!row || row->type != JSON_OBJECT) continue;
        atts[i].room_id = json_obj_get(row, "room_id");
        atts[i].id = w30_str_of(json_obj_get(row, "attachment_id"));
        atts[i].blob = w30_str_of(json_obj_get(row, "blob_id"));
        atts[i].state = w30_str_of(json_obj_get(row, "state"));
        expiry = json_obj_get(row, "expires_at");
        if (expiry && expiry->type != JSON_NULL) {
            double value = 0.0;
            if (w30_float_of(expiry, &value)) {
                atts[i].has_expiry = 1;
                atts[i].expiry = value;
            }
        }
    }
    for (i = 0; i < nb; i++) {
        const json_t *row = json_get(d.blobs, i);
        double refs = 0.0;
        if (!row || row->type != JSON_OBJECT) continue;
        blobs[i].id = w30_str_of(json_obj_get(row, "blob_id"));
        w30_num(json_obj_get(row, "ref_count"), &refs);
        blobs[i].refs = refs;
    }

    /* The committed rows of a disbanded room: expiry = disbanded_at + grace. */
    if (d.rooms && d.rooms->type == JSON_ARRAY) {
        for (i = 0; i < na; i++) {
            const json_t *room, *disbanded;
            double at = 0.0;
            if (strcmp(atts[i].state, "committed") != 0) continue;
            room = w30_first(d.rooms, "room_id", atts[i].room_id);
            if (!room) continue;
            disbanded = json_obj_get(room, "disbanded_at");
            if (!disbanded || disbanded->type == JSON_NULL) continue;
            free(atts[i].state);
            atts[i].state = strdup("disbanded");
            w30_num(disbanded, &at);
            atts[i].expiry = at + 900.0;         /* DISBANDED_GRACE_SECONDS */
            atts[i].has_expiry = 1;
        }
    }

    for (i = 0; i < na; i++) {
        if (atts[i].has_expiry && atts[i].expiry <= now) {
            atts[i].dead = 1;
            removed++;
        }
    }

    /* One pass per distinct released blob id, in first-release order. */
    for (i = 0; i < na; i++) {
        double count = 0.0;
        int already = 0;
        if (!atts[i].dead) continue;
        for (j = 0; j < nseen; j++)
            if (strcmp(seen[j], atts[i].blob) == 0) { already = 1; break; }
        if (already) continue;
        seen[nseen++] = atts[i].blob;
        for (j = 0; j < na; j++)
            if (atts[j].dead && strcmp(atts[j].blob, atts[i].blob) == 0) count += 1.0;
        for (j = 0; j < nb; j++) {
            if (blobs[j].dead || strcmp(blobs[j].id, atts[i].blob) != 0) continue;
            if (blobs[j].refs <= count) {
                blobs[j].dead = 1;
                deleted[ndel++] = blobs[j].id;
            } else {
                blobs[j].refs -= count;
            }
            break;
        }
    }

    /* Files: the released blobs are unlinked, then the sweep reclaims orphans. */
    if (d.files) {
        size_t nf = json_len(d.files);
        for (i = 0; i < nf; i++) {
            const json_t *v = json_get(d.files, i);
            const char *name;
            int live = 0;
            if (!v || v->type != JSON_STRING) continue;
            name = v->str_val ? v->str_val : "";
            for (j = 0; j < nb; j++)
                if (!blobs[j].dead && strcmp(blobs[j].id, name) == 0) live = 1;
            if (!(strncmp(name, ".tmp-", 5) == 0 || strncmp(name, ".prune-", 7) == 0 ||
                  (w30_blob_id_ok_str(name) && !live)))
                files[nfiles++] = name;
        }
    }

    qsort(deleted, ndel, sizeof(char *), w30_cmp_str);
    qsort(files, nfiles, sizeof(char *), w30_cmp_str);

    out = json_object();
    json_set(out, "removed", json_number((double)removed));
    json_set(out, "deleted_blobs", w30_string_array(deleted, ndel));
    {
        json_t *refs = json_object();
        for (i = 0; i < nb; i++)
            if (!blobs[i].dead) json_set(refs, blobs[i].id, json_number(blobs[i].refs));
        json_set(out, "ref_counts", refs);
    }
    {
        json_t *arr = json_array();
        for (i = 0; i < na; i++) {
            json_t *row;
            if (atts[i].dead) continue;
            row = json_object();
            json_set(row, "attachment_id", json_string(atts[i].id));
            json_set(row, "state", json_string(atts[i].state));
            json_set(row, "expires_at",
                     atts[i].has_expiry ? json_number(atts[i].expiry) : json_null());
            json_append(arr, row);
        }
        for (i = 0; i + 1 < json_len(arr); i++)
            for (j = i + 1; j < json_len(arr); j++) {
                const char *a = json_get_str(json_get(arr, i), "attachment_id", "");
                const char *b = json_get_str(json_get(arr, j), "attachment_id", "");
                if (strcmp(a, b) > 0) {
                    json_t *tmp = json_copy(json_get(arr, i));
                    json_array_set(arr, i, json_copy(json_get(arr, j)));
                    json_array_set(arr, j, tmp);
                }
            }
        json_set(out, "attachments", arr);
    }
    json_set(out, "files", w30_string_array(files, nfiles));

done:
    if (atts)
        for (i = 0; i < na; i++) {
            if (!atts[i].id) continue;
            free(atts[i].id);
            free(atts[i].blob);
            free(atts[i].state);
        }
    if (blobs)
        for (i = 0; i < nb; i++) free(blobs[i].id);
    free(atts);
    free(blobs);
    free(deleted);
    free(seen);
    free(files);
    return out ? w30_ok(out) : w30_ok(json_null());
}

/* ── gate p2: the canonical manifest ─────────────────────────────────────── */

static char *w30_strip_ascii(const char *s) {
    size_t n, a = 0, b;
    if (!s) return strdup("");
    n = strlen(s);
    b = n;
    while (a < n && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' ||
                     s[a] == '\f' || s[a] == '\v')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' ||
                     s[b - 1] == '\r' || s[b - 1] == '\f' || s[b - 1] == '\v')) b--;
    {
        char *out = (char *)malloc(b - a + 1);
        if (!out) return NULL;
        memcpy(out, s + a, b - a);
        out[b - a] = '\0';
        return out;
    }
}

static void w30_lower_ascii(char *s) {
    for (; s && *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

static size_t w30_char_len(const char *s) {
    size_t n = 0;
    for (; s && *s; s++)
        if (((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}

static int w30_mime_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '!' || c == '#' || c == '$' || c == '&' || c == '^' || c == '_' ||
           c == '.' || c == '+' || c == '-';
}

static int w30_mime_ok(const char *s) {
    size_t i = 0, n = strlen(s);
    int part = 0;
    if (n == 0 || n > 127) return 0;
    /* [A-Za-z0-9][A-Za-z0-9!#$&^_.+-]* '/' [A-Za-z0-9][A-Za-z0-9!#$&^_.+-]* */
    if (!((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z') ||
          (s[0] >= '0' && s[0] <= '9'))) return 0;
    for (i = 1; i < n; i++) {
        if (part == 0) {
            if (w30_mime_char(s[i])) continue;
            if (s[i] == '/') { part = 1; if (i + 1 >= n) return 0; }
            else return 0;
            if (!((s[i + 1] >= 'A' && s[i + 1] <= 'Z') || (s[i + 1] >= 'a' && s[i + 1] <= 'z') ||
                  (s[i + 1] >= '0' && s[i + 1] <= '9'))) return 0;
            i++;
            continue;
        }
        if (!w30_mime_char(s[i])) return 0;
    }
    return part == 1;
}

static const char *const W30_FIELDS[5] = {
    "attachment_id", "kind", "name", "size", "mime",
};

static int w30_in_fields(const char *key) {
    int i;
    for (i = 0; i < 5; i++)
        if (strcmp(W30_FIELDS[i], key) == 0) return 1;
    return 0;
}

static char *w30_join_fields(const char *prefix, const char *const *names, size_t n) {
    size_t total = strlen(prefix) + 1, i;
    char *out, *p;
    for (i = 0; i < n; i++) total += strlen(names[i]) + 2;
    out = (char *)malloc(total);
    if (!out) return NULL;
    p = out;
    p += sprintf(p, "%s", prefix);
    for (i = 0; i < n; i++) p += sprintf(p, "%s%s", i ? ", " : "", names[i]);
    return out;
}

/* The exact-field-set refusal of one entry, or NULL; the message is heap-owned. */
static char *w30_field_error(const json_t *entry) {
    size_t n = json_object_size(entry), i, nu = 0, nm = 0;
    const char **unknown = (const char **)calloc(n ? n : 1, sizeof *unknown);
    const char **missing = (const char **)calloc(5, sizeof *missing);
    char *msg = NULL;

    if (!unknown || !missing) {
        free(unknown);
        free(missing);
        return NULL;
    }
    for (i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(entry, i);
        if (!key) continue;
        if (!w30_in_fields(key)) unknown[nu++] = key;
    }
    for (i = 0; i < 5; i++)
        if (!json_obj_get(entry, W30_FIELDS[i])) missing[nm++] = W30_FIELDS[i];
    if (nu) {
        qsort(unknown, nu, sizeof(char *), w30_cmp_str);
        msg = w30_join_fields("attachment manifest has unknown fields: ", unknown, nu);
    } else if (nm) {
        qsort(missing, nm, sizeof(char *), w30_cmp_str);
        msg = w30_join_fields("attachment manifest is missing fields: ", missing, nm);
    }
    free(unknown);
    free(missing);
    return msg;
}

static json_t *w30_manifest_validate(const json_t *value, const char **etype,
                                     const char **emsg, char *buf, size_t cap) {
    json_t *norm;
    size_t i, n;
    long long total = 0;

    *etype = "AttachmentError";
    if (!value || value->type != JSON_ARRAY) {
        *emsg = "attachments must be a list";
        return NULL;
    }
    n = json_len(value);
    if (n > 8) {
        *emsg = "attachments must contain at most 8 entries";
        return NULL;
    }
    norm = json_array();
    for (i = 0; i < n; i++) {
        const json_t *entry = json_get(value, i);
        const json_t *size, *kind, *mime;
        long long size_value = 0;
        char *name, *lower, *id, *field_msg;
        const char *kind_value;

        if (!entry || entry->type != JSON_OBJECT) {
            json_free(norm);
            *emsg = "each attachment manifest entry must be an object";
            return NULL;
        }
        field_msg = w30_field_error(entry);
        if (field_msg) {
            snprintf(buf, cap, "%s", field_msg);
            free(field_msg);
            json_free(norm);
            *emsg = buf;
            return NULL;
        }

        size = json_obj_get(entry, "size");
        if (!size || size->type != JSON_NUMBER ||
            (double)(long long)size->num_val != size->num_val ||
            !(size->num_val > 0 && size->num_val <= 15000000.0)) {
            json_free(norm);
            *emsg = "attachment size is outside the per-file limit";
            return NULL;
        }
        size_value = (long long)size->num_val;

        kind = json_obj_get(entry, "kind");
        kind_value = (kind && kind->type == JSON_STRING && kind->str_val) ? kind->str_val : NULL;
        if (!kind_value || (strcmp(kind_value, "image") != 0 && strcmp(kind_value, "pdf") != 0 &&
                            strcmp(kind_value, "file") != 0)) {
            json_free(norm);
            *emsg = "attachment kind must be image, pdf, or file";
            return NULL;
        }

        mime = json_obj_get(entry, "mime");
        if (!mime || mime->type != JSON_STRING) {
            json_free(norm);
            *emsg = "attachment mime must be a string";
            return NULL;
        }
        lower = w30_strip_ascii(mime->str_val ? mime->str_val : "");
        w30_lower_ascii(lower);
        if (!lower || !w30_mime_ok(lower)) {
            free(lower);
            json_free(norm);
            *emsg = "attachment mime is invalid";
            return NULL;
        }
        if (strcmp(kind_value, "image") == 0 && strncmp(lower, "image/", 6) != 0) {
            free(lower);
            json_free(norm);
            *emsg = "image attachments require an image MIME type";
            return NULL;
        }
        if (strcmp(kind_value, "pdf") == 0 && strcmp(lower, "application/pdf") != 0) {
            free(lower);
            json_free(norm);
            *emsg = "PDF attachments require application/pdf";
            return NULL;
        }

        id = (json_obj_get(entry, "attachment_id") &&
              json_obj_get(entry, "attachment_id")->type == JSON_STRING)
                 ? strdup(json_obj_get(entry, "attachment_id")->str_val) : NULL;
        if (!w30_attachment_id_ok_str(id)) {
            free(id);
            free(lower);
            json_free(norm);
            *emsg = "invalid attachment_id";
            return NULL;
        }

        {
            const json_t *raw_name = json_obj_get(entry, "name");
            if (!raw_name || raw_name->type != JSON_STRING) {
                free(id);
                free(lower);
                json_free(norm);
                *emsg = "attachment name must be a string";
                return NULL;
            }
            name = w30_strip_ascii(raw_name->str_val ? raw_name->str_val : "");
        }
        {
            size_t l = w30_char_len(name);
            int bad = !name || l == 0 || l > 255 || strcmp(name, ".") == 0 ||
                      strcmp(name, "..") == 0 || strchr(name, '/') || strchr(name, '\\') ||
                      strchr(name, '\n') || strchr(name, '\r');
            if (bad) {
                free(name);
                free(id);
                free(lower);
                json_free(norm);
                *emsg = "attachment name must be a bounded basename";
                return NULL;
            }
        }

        {
            json_t *row = json_object();
            json_set(row, "attachment_id", json_string(id));
            json_set(row, "kind", json_string(kind_value));
            json_set(row, "name", json_string(name));
            json_set(row, "size", json_number((double)size_value));
            json_set(row, "mime", json_string(lower));
            json_append(norm, row);
        }
        total += size_value;
        free(name);
        free(id);
        free(lower);
    }

    for (i = 0; i < n; i++) {
        const char *id = json_get_str(json_get(norm, i), "attachment_id", "");
        size_t j;
        for (j = 0; j < i; j++)
            if (strcmp(json_get_str(json_get(norm, j), "attachment_id", ""), id) == 0) {
                json_free(norm);
                *emsg = "attachment ids must be unique within one message";
                return NULL;
            }
    }
    if (total > 25000000) {
        json_free(norm);
        *emsg = "attachments exceed the per-message total size limit";
        return NULL;
    }
    return norm;
}

json_t *w30_hostedroom_fe22db_p2_manifest_validate(const json_t *in) {
    const char *etype = NULL, *emsg = NULL;
    char buf[256];
    json_t *norm = w30_manifest_validate(w30_get(in, "manifest"), &etype, &emsg, buf, sizeof buf);
    if (!norm) return w30_err_json(etype, emsg);
    return w30_ok(norm);
}

/* ── gate p2: retention inside the event append ──────────────────────────── */

typedef struct {
    char *id;
    const json_t *state;
    int has_expiry;
    double expiry;
    int has_updated;
    double updated;
} w30_retained;

static json_t *w30_attachment_rows(const w30_db *d, const json_t *norm, double now) {
    w30_retained *rows = NULL;
    size_t nr = 0, i, n;
    json_t *arr = json_array();

    n = d->attachments ? json_len(d->attachments) : 0;
    rows = (w30_retained *)calloc(n ? n : 1, sizeof *rows);
    if (!rows) return arr;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(d->attachments, i);
        const json_t *expiry, *updated;
        if (!row || row->type != JSON_OBJECT) continue;
        rows[nr].id = w30_str_of(json_obj_get(row, "attachment_id"));
        rows[nr].state = json_obj_get(row, "state");
        expiry = json_obj_get(row, "expires_at");
        if (expiry && expiry->type != JSON_NULL) {
            double value = 0.0;
            if (w30_float_of(expiry, &value)) {
                rows[nr].has_expiry = 1;
                rows[nr].expiry = value;
            }
        }
        updated = json_obj_get(row, "updated_at");
        if (updated && updated->type != JSON_NULL) {
            double value = 0.0;
            if (w30_float_of(updated, &value)) {
                rows[nr].has_updated = 1;
                rows[nr].updated = value;
            }
        }
        if (norm) {
            size_t k;
            for (k = 0; k < json_len(norm); k++)
                if (strcmp(json_get_str(json_get(norm, k), "attachment_id", ""), rows[nr].id) == 0) {
                    rows[nr].has_expiry = 0;
                    rows[nr].expiry = 0.0;
                    rows[nr].has_updated = 1;
                    rows[nr].updated = now;
                }
        }
        nr++;
    }
    /* ORDER BY attachment_id */
    for (i = 0; i + 1 < nr; i++) {
        size_t j, best = i;
        for (j = i + 1; j < nr; j++)
            if (strcmp(rows[j].id, rows[best].id) < 0) best = j;
        if (best != i) {
            w30_retained tmp = rows[i];
            rows[i] = rows[best];
            rows[best] = tmp;
        }
    }
    for (i = 0; i < nr; i++) {
        json_t *row = json_object();
        json_set(row, "attachment_id", json_string(rows[i].id));
        json_set(row, "state", json_string(w30_str_of(rows[i].state)));
        json_set(row, "expires_at", rows[i].has_expiry ? json_number(rows[i].expiry) : json_null());
        json_set(row, "updated_at", rows[i].has_updated ? json_number(rows[i].updated) : json_null());
        json_append(arr, row);
        free(rows[i].id);
    }
    free(rows);
    return arr;
}

json_t *w30_hostedroom_fe22db_p2_retain(const json_t *in) {
    const char *etype = NULL, *emsg = NULL;
    char buf[256];
    json_t *norm, *out;
    w30_db d;
    double now = 0.0;
    size_t i, n;

    norm = w30_manifest_validate(w30_get(in, "manifest"), &etype, &emsg, buf, sizeof buf);
    if (!norm) return w30_err_json(etype, emsg);

    w30_db_load(&d, w30_get(in, "db"));
    w30_num(w30_get(in, "now"), &now);
    n = json_len(norm);
    if (n) {
        const json_t *raw = w30_get(in, "manifest");
        for (i = 0; i < n; i++) {
            if (!w30_eq(json_get(norm, i), json_get(raw, i))) {
                json_free(norm);
                return w30_err_json("AttachmentError",
                                    "published attachment metadata must be canonical");
            }
        }
        if (!d.has_attachments) {
            json_free(norm);
            return w30_err_json("EventAttachmentConflictError", NULL);
        }
        for (i = 0; i < n; i++) {
            const json_t *entry = json_get(norm, i);
            const char *id = json_get_str(entry, "attachment_id", "");
            const json_t *row = w30_first_str(d.attachments, "attachment_id", id);
            const json_t *expiry;
            double access = 0.0;
            int conflict = 0;
            if (!row) conflict = 1;
            else if (!w30_eq(json_obj_get(row, "room_id"), w30_get(in, "room_id"))) conflict = 1;
            else if (!w30_eq(json_obj_get(row, "event_id"), w30_get(in, "event_id"))) conflict = 1;
            else if (!w30_eq(json_obj_get(row, "state"), json_string("committed"))) conflict = 1;
            else {
                w30_num(json_obj_get(row, "viewer_access"), &access);
                if (access != 1.0) conflict = 1;
            }
            if (!conflict) {
                expiry = json_obj_get(row, "expires_at");
                if (expiry && expiry->type != JSON_NULL) {
                    double at = 0.0;
                    if (w30_float_of(expiry, &at) && at <= now) conflict = 1;
                }
            }
            if (!conflict) {
                size_t k, sz = json_object_size(entry);
                for (k = 0; k < sz; k++) {
                    const char *key = json_object_get_key_at(entry, k);
                    if (!key) continue;
                    if (!w30_eq(json_obj_get(row, key), json_object_get_at(entry, k))) {
                        conflict = 1;
                        break;
                    }
                }
            }
            if (conflict) {
                json_free(norm);
                return w30_err_json("EventAttachmentConflictError", NULL);
            }
        }
    }
    out = json_object();
    json_set(out, "rows", w30_attachment_rows(&d, n ? norm : NULL, now));
    json_free(norm);
    return w30_ok(out);
}

/* ── gate p2: constants ──────────────────────────────────────────────────── */

json_t *w30_hostedroom_fe22db_p2_constants(const json_t *in) {
    json_t *o = json_object();
    json_t *cols = json_array();
    (void)in;
    json_set(o, "max_attachments_per_message", json_number(8));
    json_set(o, "max_attachment_bytes", json_number(15000000));
    json_set(o, "max_message_attachment_bytes", json_number(25000000));
    json_set(o, "max_room_attachment_bytes", json_number(536870912));
    json_set(o, "max_gateway_blob_bytes", json_number(2147483648.0));
    json_set(o, "max_room_attachment_count", json_number(50000));
    json_set(o, "max_gateway_attachment_count", json_number(200000));
    json_set(o, "max_room_uncommitted_bytes", json_number(104857600));
    json_set(o, "max_room_uncommitted_count", json_number(64));
    json_set(o, "uncommitted_ttl_seconds", json_number(3600));
    json_set(o, "disbanded_grace_seconds", json_number(900));
    json_append(cols, json_string("catalog_name"));
    json_append(cols, json_string("viewer_access"));
    json_set(o, "current_columns", cols);
    return w30_ok(o);
}

/* ── gate p2: the viewer authority fence ─────────────────────────────────── */

json_t *w30_hostedroom_fe22db_p2_require_viewer_room(const json_t *in) {
    w30_db d;
    const json_t *room;
    w30_db_load(&d, w30_get(in, "db"));
    if (!d.rooms || d.rooms->type != JSON_ARRAY)
        return w30_err_json("AttachmentNotFoundError", "Group Chat is unavailable to viewers");
    room = w30_first(d.rooms, "room_id", w30_get(in, "room_id"));
    if (!room) return w30_err_json("AttachmentNotFoundError", "Group Chat viewer authority changed");
    {
        const json_t *disbanded = json_obj_get(room, "disbanded_at");
        if (disbanded && disbanded->type != JSON_NULL)
            return w30_err_json("AttachmentNotFoundError", "Group Chat viewer authority changed");
    }
    if (!w30_eq(json_obj_get(room, "authority_gateway_id"), w30_get(in, "authority_gateway_id")) ||
        !w30_eq(json_obj_get(room, "authority_epoch"), w30_get(in, "authority_epoch")))
        return w30_err_json("AttachmentNotFoundError", "Group Chat viewer authority changed");
    if (d.quarantine && d.quarantine->type == JSON_ARRAY &&
        w30_first(d.quarantine, "room_id", w30_get(in, "room_id")))
        return w30_err_json("AttachmentNotFoundError", "Group Chat is unavailable to viewers");
    if (d.fences && d.fences->type == JSON_ARRAY &&
        w30_first(d.fences, "room_id", w30_get(in, "room_id")))
        return w30_err_json("AttachmentNotFoundError", "Group Chat is unavailable to viewers");
    return w30_ok(json_null());
}

/* ── gate p2: the committed/expired/owned/published/recipient fences ────── */

/* ``{attachment_id, kind, name, size, mime}`` from a row, as a payload item. */
static int w30_published_has(const json_t *published, const json_t *row) {
    char *id, *kind, *name, *mime;
    long long size = 0;
    size_t i, n;
    int found = 0;

    if (!published || published->type != JSON_ARRAY) return 0;
    id = w30_str_of(w30_get(row, "attachment_id"));
    kind = w30_str_of(w30_get(row, "kind"));
    name = w30_str_of(w30_get(row, "name"));
    mime = w30_str_of(w30_get(row, "mime"));
    w30_int_of(w30_get(row, "size"), &size);
    n = json_len(published);
    for (i = 0; i < n && !found; i++) {
        const json_t *item = json_get(published, i);
        const json_t *aid, *ikind, *iname, *imime, *isize;
        if (!item || item->type != JSON_OBJECT || json_object_size(item) != 5) continue;
        aid = json_obj_get(item, "attachment_id");
        ikind = json_obj_get(item, "kind");
        iname = json_obj_get(item, "name");
        imime = json_obj_get(item, "mime");
        isize = json_obj_get(item, "size");
        if (!aid || !ikind || !iname || !imime || !isize) continue;
        if (aid->type != JSON_STRING || ikind->type != JSON_STRING ||
            iname->type != JSON_STRING || imime->type != JSON_STRING) continue;
        if (strcmp(aid->str_val ? aid->str_val : "", id) != 0) continue;
        if (strcmp(ikind->str_val ? ikind->str_val : "", kind) != 0) continue;
        if (strcmp(iname->str_val ? iname->str_val : "", name) != 0) continue;
        if (strcmp(imime->str_val ? imime->str_val : "", mime) != 0) continue;
        {
            double isz = 0.0;
            if (!w30_num(isize, &isz) || isz != (double)size) continue;
        }
        found = 1;
    }
    free(id);
    free(kind);
    free(name);
    free(mime);
    return found;
}

static int w30_recipients_have(const json_t *recipients, const char *who) {
    size_t i, n;
    if (!recipients) return 0;
    if (recipients->type == JSON_ARRAY) {
        n = json_len(recipients);
        for (i = 0; i < n; i++) {
            const json_t *v = json_get(recipients, i);
            if (v && v->type == JSON_STRING && strcmp(v->str_val ? v->str_val : "", who) == 0)
                return 1;
        }
        return 0;
    }
    if (recipients->type == JSON_OBJECT) {
        n = json_object_size(recipients);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(recipients, i);
            if (k && strcmp(k, who) == 0) return 1;
        }
        return 0;
    }
    if (recipients->type == JSON_STRING)
        return strstr(recipients->str_val ? recipients->str_val : "", who) != NULL;
    return 0;
}

json_t *w30_hostedroom_fe22db_p2_read_gate(const json_t *in) {
    w30_db d;
    const json_t *row = NULL;
    const json_t *event_id;
    const json_t *expiry;
    const char *etype = NULL;
    char *event, *room;
    double now = 0.0, access = 0.0;
    int viewer = w30_truthy(w30_get(in, "viewer"));
    int reconciled = w30_truthy(w30_get(in, "reconciled"));
    json_t *out, *recipients = NULL;

    w30_db_load(&d, w30_get(in, "db"));
    if (d.has_attachments && d.attachments)
        row = w30_first2(d.attachments, "attachment_id", w30_get(in, "attachment_id"),
                         "room_id", w30_get(in, "room_id"));
    if (!row || !w30_eq(json_obj_get(row, "state"), json_string("committed")))
        return w30_err_json("AttachmentNotFoundError", "attachment is not committed for this room");

    w30_num(w30_get(in, "now"), &now);

    expiry = json_obj_get(row, "expires_at");
    if (expiry && expiry->type != JSON_NULL) {
        double at = 0.0;
        if (w30_float_of(expiry, &at) && at <= now) {
            if (reconciled) return w30_err_json("AttachmentNotFoundError", "attachment has expired");
            {
                const char *sig = NULL;
                if (!w30_named_by_owner(&d, row, &sig))
                    return w30_err_json("AttachmentNotFoundError", "attachment has expired");
            }
        }
    }

    event = w30_str_or_empty(json_obj_get(row, "event_id"));
    event_id = w30_get(in, "event_id");
    if (!w30_param_null(event_id)) {
        int same = event_id->type == JSON_STRING &&
                   strcmp(event, event_id->str_val ? event_id->str_val : "") == 0;
        if (!same) {
            free(event);
            return w30_err_json("AttachmentNotFoundError",
                                "attachment is not owned by this room event");
        }
    }

    w30_num(json_obj_get(row, "viewer_access"), &access);
    if (access != 0.0) {
        const json_t *owner, *payload, *published;
        json_t *parsed = NULL;
        char *raw, *perr = NULL;
        if (!d.events || d.events->type != JSON_ARRAY) {
            free(event);
            return w30_err_json("AttachmentNotFoundError",
                                "attachment is not published in this room");
        }
        room = w30_str_of(w30_get(in, "room_id"));
        owner = w30_event_owner(d.events, room, event);
        free(room);
        if (owner) {
            raw = w30_str_of(json_obj_get(owner, "payload_json"));
            parsed = json_parse(raw, &perr);
            if (perr) free(perr);
            free(raw);
        }
        payload = parsed;
        published = (payload && payload->type == JSON_OBJECT)
                        ? json_obj_get(payload, "attachments") : NULL;
        if (!published || published->type != JSON_ARRAY || !w30_published_has(published, row)) {
            json_free(parsed);
            free(event);
            return w30_err_json("AttachmentNotFoundError",
                                "attachment is not published in this room");
        }
        json_free(parsed);
    }

    {
        char *raw = w30_str_of(json_obj_get(row, "recipient_member_ids_json"));
        char *perr = NULL;
        recipients = raw ? json_parse(raw, &perr) : NULL;
        if (perr) free(perr);
        free(raw);
        if (!recipients) {
            free(event);
            return w30_err_json("JSONDecodeError", NULL);
        }
    }
    if (viewer) {
        if (access != 1.0) {
            json_free(recipients);
            free(event);
            return w30_err_json("AttachmentNotFoundError",
                                "attachment is unavailable to Group Chat viewers");
        }
    } else {
        char *who = w30_str_or_empty(w30_get(in, "recipient_member_id"));
        int listed = w30_recipients_have(recipients, who);
        free(who);
        if (!listed) {
            json_free(recipients);
            free(event);
            return w30_err_json("AttachmentNotFoundError",
                                "attachment is unavailable to this recipient");
        }
    }
    json_free(recipients);
    free(event);

    out = w30_envelope(row, 0, &etype);
    if (!out) return w30_err_json(etype, NULL);
    return w30_ok(out);
}

/* ── gate p2: the slice-length fence ─────────────────────────────────────── */

json_t *w30_hostedroom_fe22db_p2_range_slice(const json_t *in) {
    const json_t *blob = w30_get(in, "blob_id");
    const json_t *file_size = w30_get(in, "file_size");
    long long size = 0, offset = 0, length = 0, declared = 0, need;
    json_t *out;

    if (!blob || blob->type != JSON_STRING) return w30_err_json("TypeError", NULL);
    if (!w30_blob_id_ok_str(blob->str_val ? blob->str_val : ""))
        return w30_err_json("AttachmentIntegrityError", "stored blob id is invalid");
    w30_int_of(w30_get(in, "size"), &size);
    w30_int_of(w30_get(in, "offset"), &offset);
    w30_int_of(w30_get(in, "length"), &length);
    if (w30_param_null(file_size))
        return w30_err_json("AttachmentIntegrityError", "canonical attachment blob is unavailable");
    if (!w30_int_of(file_size, &declared) || declared != size)
        return w30_err_json("AttachmentIntegrityError", "canonical attachment blob size changed");
    need = size - offset;
    if (length < need) need = length;
    if (need < 0) return w30_err_json("AttachmentIntegrityError",
                                      "canonical attachment blob size changed");
    out = json_object();
    json_set(out, "bytes", json_number((double)need));
    return w30_ok(out);
}
