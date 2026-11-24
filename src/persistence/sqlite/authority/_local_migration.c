/*
 * _local_migration.c -- real C11 port of the legacy arm of a local reservation:
 * upstream Hermes persistence/sqlite/authority/_local_migration.py.
 *
 * PoP: w20_localmigra_5dcdb5_binding_prefix @ persistence/sqlite/authority/_binding_history.py:BINDING_PREFIX
 * PoP: w20_localmigra_5dcdb5_legacy_prefix @ persistence/sqlite/conversation/_store/local_migration.py:LEGACY_PREFIX
 * PoP: w20_localmigra_5dcdb5_canonical_dumps @ persistence/core/canonical.py:canonical_dumps
 * PoP: w20_localmigra_5dcdb5_canonical_json @ persistence/core/canonical.py:canonical_json
 * PoP: w20_localmigra_5dcdb5_bind_legacy_target @ persistence/sqlite/authority/_local_migration.py:bind_legacy_target
 *
 * The connection, the holder, the owner rule and the filesystem all stay
 * upstream: every collaborator's answer arrives in the scene object, and the
 * port runs the real guard ladder and shapes the two statements verbatim.
 */
#include "hermes_json.h"
#include "noc_w20_localmigra_5dcdb5.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- growable byte buffer ------------------------------------------------- */

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} lm5cdb_sb;

static void lm5cdb_sb_init(lm5cdb_sb *b) {
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
}

static int lm5cdb_sb_grow(lm5cdb_sb *b, size_t extra) {
    size_t need = b->n + extra + 1;
    size_t cap;
    char *q;
    if (need <= b->cap) return 1;
    cap = b->cap ? b->cap : 64;
    while (cap < need) cap *= 2;
    q = (char *)realloc(b->p, cap);
    if (!q) return 0;
    b->p = q;
    b->cap = cap;
    return 1;
}

static void lm5cdb_sb_putn(lm5cdb_sb *b, const char *s, size_t n) {
    if (!lm5cdb_sb_grow(b, n)) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void lm5cdb_sb_puts(lm5cdb_sb *b, const char *s) {
    if (!s) return;
    lm5cdb_sb_putn(b, s, strlen(s));
}

static void lm5cdb_sb_putc(lm5cdb_sb *b, char c) {
    lm5cdb_sb_putn(b, &c, 1);
}

static char *lm5cdb_sb_take(lm5cdb_sb *b) {
    char *r = b->p;
    if (!r) {
        r = (char *)malloc(1);
        if (r) r[0] = '\0';
    }
    b->p = NULL;
    b->n = b->cap = 0;
    return r;
}

/* canonical_dumps: ensure_ascii=False, sort_keys=True, separators=(",", ":"). */
static void lm5cdb_put_string(lm5cdb_sb *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char buf[8];
    lm5cdb_sb_putc(b, '"');
    while (*p) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') {
            lm5cdb_sb_putc(b, '\\');
            lm5cdb_sb_putc(b, (char)c);
            p++;
        } else if (c == '\n') { lm5cdb_sb_puts(b, "\\n"); p++; }
        else if (c == '\r') { lm5cdb_sb_puts(b, "\\r"); p++; }
        else if (c == '\t') { lm5cdb_sb_puts(b, "\\t"); p++; }
        else if (c == '\b') { lm5cdb_sb_puts(b, "\\b"); p++; }
        else if (c == '\f') { lm5cdb_sb_puts(b, "\\f"); p++; }
        else if (c < 0x20) {
            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
            lm5cdb_sb_puts(b, buf);
            p++;
        } else {
            lm5cdb_sb_putc(b, (char)c);         /* non-ASCII stays literal */
            p++;
        }
    }
    lm5cdb_sb_putc(b, '"');
}

static void lm5cdb_put_number(lm5cdb_sb *b, double d) {
    char buf[40];
    if (!isfinite(d)) {
        /* allow_nan=False: canonical_json refuses NaN/inf; callers see the
         * refusal on the token, never a stored value. */
        lm5cdb_sb_puts(b, d != d ? "NaN" : (d > 0 ? "Infinity" : "-Infinity"));
        return;
    }
    if (floor(d) == d && d >= -9.007199254740992e15 && d <= 9.007199254740992e15) {
        snprintf(buf, sizeof(buf), "%lld", (long long)d);
    } else {
        snprintf(buf, sizeof(buf), "%.17g", d);
    }
    lm5cdb_sb_puts(b, buf);
}

typedef struct {
    const char *key;
    const json_t *val;
} lm5cdb_kv;

static int lm5cdb_cmp_kv(const void *a, const void *b) {
    const lm5cdb_kv *ka = (const lm5cdb_kv *)a;
    const lm5cdb_kv *kb = (const lm5cdb_kv *)b;
    return strcmp(ka->key ? ka->key : "", kb->key ? kb->key : "");
}

static void lm5cdb_dumps(lm5cdb_sb *b, const json_t *v) {
    size_t i;
    if (!v) { lm5cdb_sb_puts(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL:   lm5cdb_sb_puts(b, "null"); break;
    case JSON_BOOL:   lm5cdb_sb_puts(b, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: lm5cdb_put_number(b, v->num_val); break;
    case JSON_STRING: lm5cdb_put_string(b, v->str_val); break;
    case JSON_ARRAY:
        lm5cdb_sb_putc(b, '[');
        for (i = 0; i < v->c.count; i++) {
            if (i) lm5cdb_sb_putc(b, ',');
            lm5cdb_dumps(b, v->c.items[i]);
        }
        lm5cdb_sb_putc(b, ']');
        break;
    case JSON_OBJECT: {
        size_t n = v->c.count;
        lm5cdb_kv *rows;
        lm5cdb_sb_putc(b, '{');
        if (n == 0) { lm5cdb_sb_putc(b, '}'); break; }
        rows = (lm5cdb_kv *)calloc(n, sizeof(*rows));
        if (!rows) { lm5cdb_sb_putc(b, '}'); break; }
        for (i = 0; i < n; i++) {
            rows[i].key = v->c.keys[i];
            rows[i].val = v->c.items[i];
        }
        qsort(rows, n, sizeof(*rows), lm5cdb_cmp_kv);
        for (i = 0; i < n; i++) {
            if (i) lm5cdb_sb_putc(b, ',');
            lm5cdb_put_string(b, rows[i].key);
            lm5cdb_sb_putc(b, ':');
            lm5cdb_dumps(b, rows[i].val);
        }
        free(rows);
        lm5cdb_sb_putc(b, '}');
        break;
    }
    }
}

json_t *w20_localmigra_5dcdb5_canonical_dumps(const json_t *value) {
    lm5cdb_sb b;
    char *text;
    json_t *out;
    lm5cdb_sb_init(&b);
    lm5cdb_dumps(&b, value);
    text = lm5cdb_sb_take(&b);
    out = json_string(text ? text : "");
    free(text);
    return out;
}

json_t *w20_localmigra_5dcdb5_canonical_json(const json_t *value) {
    json_t *out = json_object();
    if (!value || value->type != JSON_OBJECT) {
        json_set(out, "refusal", json_string("invalid_params"));
        return out;
    }
    json_set(out, "value", w20_localmigra_5dcdb5_canonical_dumps(value));
    return out;
}

/* Heap copy of canonical_dumps(value); the caller frees it. */
static char *lm5cdb_canon_text(const json_t *value) {
    json_t *node = w20_localmigra_5dcdb5_canonical_dumps(value);
    const char *s = (node && node->str_val) ? node->str_val : "";
    char *out = (char *)malloc(strlen(s) + 1);
    if (out) strcpy(out, s);
    json_free(node);
    return out;
}

/* --- constants ------------------------------------------------------------ */

const char *w20_localmigra_5dcdb5_binding_prefix(void) {
    return "gateway.history_control.v1.";
}

const char *w20_localmigra_5dcdb5_legacy_prefix(void) {
    return "gateway.local_legacy.v1:";
}

#define LM5CDB_UPDATE_SQL \
    "UPDATE sessions SET source=?,session_key=?,chat_id=?,user_id=?,chat_type=?,origin_json=? WHERE id=?"
#define LM5CDB_STATE_SQL \
    "INSERT INTO state_meta(key,value) VALUES(?,?)"

/* --- answer helpers ------------------------------------------------------- */

static json_t *lm5cdb_refused(const char *reason) {
    json_t *out = json_object();
    json_set(out, "refusal", json_string(reason));
    return out;
}

static json_t *lm5cdb_raised(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "raised", json_string(type));
    json_set(out, "message", json_string(message));
    return out;
}

/* `receipt[key]` -- the Python body reads it positionally, so a missing key is
 * a KeyError whose message is repr(key). */
static json_t *lm5cdb_key_error(const char *key) {
    char msg[128];
    snprintf(msg, sizeof(msg), "'%s'", key ? key : "");
    return lm5cdb_raised("KeyError", msg);
}

static int lm5cdb_truthy(const json_t *v) {
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

/* A scene string field, "" when absent or not a string. */
static const char *lm5cdb_scene_str(const json_t *scene, const char *key) {
    json_t *v = json_obj_get(scene, key);
    if (!v || v->type != JSON_STRING) return "";
    return v->str_val ? v->str_val : "";
}

/* --- bind_legacy_target --------------------------------------------------- */

json_t *w20_localmigra_5dcdb5_bind_legacy_target(const json_t *receipt,
                                                 const json_t *scene) {
    const char *target, *session_id, *principal_id, *profile_id, *route, *label;
    json_t *row, *imported, *entry, *origin, *statements, *out, *meta;
    char *origin_text, *meta_text, *key;
    size_t key_len;

    if (!receipt || receipt->type != JSON_OBJECT) return lm5cdb_key_error("legacy_session_id");
    if (!json_obj_get(receipt, "legacy_session_id")) return lm5cdb_key_error("legacy_session_id");
    target = json_get_str(receipt, "legacy_session_id", NULL);
    if (!target) return lm5cdb_key_error("legacy_session_id");
    if (!json_obj_get(receipt, "session_id")) return lm5cdb_key_error("session_id");
    session_id = json_get_str(receipt, "session_id", "");
    if (strcmp(target, session_id ? session_id : "") != 0) return lm5cdb_refused("storage_unavailable");

    row = json_obj_get(scene, "row");
    if (!row || row->type == JSON_NULL) return lm5cdb_refused("not_found");

    {
        const char *verdict = lm5cdb_scene_str(scene, "validate");
        if (verdict[0] && strcmp(verdict, "ok") != 0) return lm5cdb_refused(verdict);
    }

    imported = json_obj_get(scene, "imported");
    if (imported && imported->type != JSON_NULL && lm5cdb_truthy(imported)) {
        if (!json_get_bool(scene, "binding_ok", 0)) return lm5cdb_refused("permission_denied");
    }

    if (json_get_bool(scene, "chain_step", 0)) return lm5cdb_refused("admission_conflict");

    {
        const char *verdict = lm5cdb_scene_str(scene, "idle");
        if (verdict[0] && strcmp(verdict, "ok") != 0) return lm5cdb_refused(verdict);
    }

    if (json_get_bool(scene, "lease", 0)) return lm5cdb_refused("runtime_coordination_required");
    if (json_get_bool(scene, "legacy_key", 0)) return lm5cdb_refused("storage_unavailable");

    entry = json_obj_get(receipt, "entry");
    if (!entry) return lm5cdb_key_error("entry");
    if (entry->type != JSON_OBJECT) {
        return lm5cdb_raised("TypeError", "string indices must be integers, not 'str'");
    }
    origin = json_obj_get(entry, "origin");
    if (!origin) return lm5cdb_key_error("origin");

    /* Binding is a read-side claim: an owned local row keeps main's source
     * label; a client app's own label becomes the Desktop chat it resumes as. */
    label = "desktop";
    if (!json_get_bool(scene, "client_row", 0)) {
        json_t *src = json_obj_get(row, "source");
        if (src && src->type == JSON_STRING && src->str_val && src->str_val[0]) label = src->str_val;
    }

    if (!json_obj_get(receipt, "route")) return lm5cdb_key_error("route");
    route = json_get_str(receipt, "route", "");
    if (!json_obj_get(receipt, "principal_id")) return lm5cdb_key_error("principal_id");
    principal_id = json_get_str(receipt, "principal_id", "");
    if (!json_obj_get(receipt, "profile_id")) return lm5cdb_key_error("profile_id");
    profile_id = json_get_str(receipt, "profile_id", "");

    origin_text = lm5cdb_canon_text(origin);

    meta = json_object();
    json_set(meta, "session_id", json_string(session_id ? session_id : ""));
    json_set(meta, "profile_id", json_string(profile_id));
    json_set(meta, "principal_id", json_string(principal_id));
    json_set(meta, "legacy_session_id", json_string(target));
    meta_text = lm5cdb_canon_text(meta);
    json_free(meta);

    key_len = strlen(w20_localmigra_5dcdb5_legacy_prefix()) + strlen(target) + 1;
    key = (char *)malloc(key_len);
    if (key) snprintf(key, key_len, "%s%s", w20_localmigra_5dcdb5_legacy_prefix(), target);

    statements = json_array();
    {
        json_t *stmt = json_object();
        json_t *params = json_array();
        json_append(params, json_string(label));
        json_append(params, json_string(route));
        json_append(params, json_string(session_id ? session_id : ""));
        json_append(params, json_string(principal_id));
        json_append(params, json_string("dm"));
        json_append(params, json_string(origin_text ? origin_text : ""));
        json_append(params, json_string(target));
        json_set(stmt, "sql", json_string(LM5CDB_UPDATE_SQL));
        json_set(stmt, "params", params);
        json_append(statements, stmt);
    }
    {
        json_t *stmt = json_object();
        json_t *params = json_array();
        json_append(params, json_string(key ? key : ""));
        json_append(params, json_string(meta_text ? meta_text : ""));
        json_set(stmt, "sql", json_string(LM5CDB_STATE_SQL));
        json_set(stmt, "params", params);
        json_append(statements, stmt);
    }

    free(origin_text);
    free(meta_text);
    free(key);

    out = json_object();
    json_set(out, "statements", statements);
    return out;
}
