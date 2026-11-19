/*
 * conversation_store_flow.c — carried-row coverage-proof FLOW cores of the
 * conversation store. The named target persistence/conversation/store.py does
 * not exist in the quarry; it resolves to the real defining module
 *   persistence/sqlite/conversation/_store/coverage.py
 * ("Carried-row coverage proofs for compaction"). All 19 functions are ported
 * and credited there (each was verified UNCLAIMED by PoP grep, by name and by
 * module). The store's pure helpers decide which held (compacted) dicts already
 * carry which durable tail rows, so a commit clones a concurrent append once,
 * never twice, and never drops a stale version.
 *
 * Decision sequences only: every collaborator the module imports or reads from
 * the DB enters through ONE universal seam: cst_call(ctx, name, args). The
 * harness wires it to a recording stub fed the case's scripted results (in.io);
 * the oracle installs recording stubs emitting the identical event shape
 * {"name","args","ret"} from the SAME scripts. Seamed collaborators:
 *   _row_identity, _parse_tool_calls, _decode_content, _encode_content,
 *   _loaded_view_content, _serialized_message_row   (_store/messages.py, claimed)
 *   message_uid_or_none                             (session_rules.turns)
 *   _is_image_part, coerce_epoch,
 *   strip_leading_message_timestamps                (_store/rules.py)
 *   transcript_row_snapshot                         (_store/transcript_repair.py)
 *   _placeholders, _id_chunks                       (_store/sql_common.py)
 *   conn.execute                                    (the sqlite connection)
 * Constants (MESSAGE_UID, DB_ROW_SNAPSHOT, ABSORBED_ROW_IDS, REPLAY_SIDECARS,
 * MERGED_DURABLE_ROWS, UNNAMED_DURABLE_ROWS, RETIRED_DURABLE_ROWS, RETIRED_ROW,
 * OWN_ROW, _DB_PERSISTED_MARKER) are seeded, not seamed.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and cst_-prefixed.
 *
 * Representation contract (shared with tests/oracle_conversation_store_flow.py):
 * - NULL json_t* == an absent key; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc)} object.
 * - Plain JSON numbers are Python ints; JSON bools are bools. A Python float
 *   travels as {"__float__": true, "v": <number>}; a Python tuple as
 *   {"__tuple__": true, "v": [...]}. Row identities (_row_identity seam returns)
 *   travel as plain JSON arrays (lists); the oracle scripts them as lists so
 *   identity equality is list==list on both sides.
 * - A sqlite3.Row is a plain JSON object (row["col"] reads). conn.execute is a
 *   seam returning the scripted row list; .fetchall() is inert on both sides.
 * - holder is an opaque token passed through to seams (fixtures use null).
 * - Sets return as SORTED arrays (canon sorts set/frozenset on both sides);
 *   tuples fold to arrays and integral floats to ints in canon. Dict key order
 *   is irrelevant (canon compares parsed JSON objects by content).
 * - Fixture domain: identity components are str/int/float/None/list (no bool,
 *   no nested tuple); timestamps are int/float/None; text is ASCII; covered /
 *   tail / row ids are positive ints; no literal "__float__"/"__tuple__" keys
 *   appear inside content/row trees.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct cst_ctx {
    json_t *state;      /* owned; coverage never mutates inputs, stays empty */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} cst_ctx_t;

/* ── constants (seeded, not seamed) ── */
#define CST_MESSAGE_UID        "message_uid"
#define CST_DB_ROW_SNAPSHOT    "_db_row_snapshot"
#define CST_ABSORBED_ROW_IDS   "_absorbed_row_ids"
#define CST_MERGED_DURABLE     "_merged_durable_rows"
#define CST_UNNAMED_DURABLE    "_unnamed_durable_rows"
#define CST_RETIRED_DURABLE    "_retired_durable_rows"
#define CST_RETIRED_ROW        "_retired_row"
#define CST_OWN_ROW            "_own_row"
#define CST_DB_PERSISTED       "_db_persisted"
static const char *CST_REPLAY_SIDECARS[] = {
    "reasoning", "reasoning_content", "reasoning_details",
    "codex_reasoning_items", "codex_message_items"
};
#define CST_REPLAY_SIDECARS_N 5
#define CST_SQL_IN_CHUNK 900

/* ── basic type helpers ── */
static bool cst_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static bool cst_is_float_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__float__", false);
}
static bool cst_is_tuple_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__tuple__", false);
}
static bool cst_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool cst_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool cst_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool cst_is_int(const json_t *v) { return v && v->type == JSON_NUMBER; }
/* isinstance(x, int) — bool IS an int instance */
static bool cst_isinstance_int(const json_t *v) { return cst_is_int(v) || cst_is_bool(v); }
/* type(x) is int — excludes bool */
static bool cst_type_is_int(const json_t *v) { return cst_is_int(v); }
static bool cst_is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool cst_is_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !cst_is_float_tag(v) && !cst_is_tuple_tag(v);
}
static bool cst_is_numlike(const json_t *v) {
    return cst_is_int(v) || cst_is_bool(v) || cst_is_float_tag(v);
}
static double cst_num(const json_t *v) {
    if (cst_is_float_tag(v)) return json_get_num(v, "v", 0);
    if (cst_is_bool(v)) return v->bool_val ? 1.0 : 0.0;
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static const char *cst_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool cst_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (cst_is_float_tag(v)) return json_get_num(v, "v", 0) != 0.0;
    if (cst_is_tuple_tag(v)) {
        const json_t *it = json_obj_get(v, "v");
        return it && it->type == JSON_ARRAY && it->c.count > 0;
    }
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const json_t *cst_seq_items(const json_t *v) {
    if (cst_is_tuple_tag(v)) return json_obj_get(v, "v");
    return v;
}
static bool cst_is_seq(const json_t *v) {
    return cst_is_list(v) || cst_is_tuple_tag(v);
}
static size_t cst_seq_len(const json_t *v) {
    const json_t *it = cst_seq_items(v);
    return (it && it->type == JSON_ARRAY) ? it->c.count : 0;
}
static const json_t *cst_seq_get(const json_t *v, size_t i) {
    const json_t *it = cst_seq_items(v);
    return (it && it->type == JSON_ARRAY) ? json_get(it, i) : NULL;
}

/* Python == on the representation (numeric cross-type, deep list/dict). */
static bool cst_eq(const json_t *a, const json_t *b) {
    bool an = cst_is_numlike(a), bn = cst_is_numlike(b);
    if (an || bn) return an && bn && cst_num(a) == cst_num(b);
    if (cst_is_null(a) || cst_is_null(b)) return cst_is_null(a) && cst_is_null(b);
    bool as = cst_is_str(a), bs = cst_is_str(b);
    if (as || bs) return as && bs && strcmp(a->str_val, b->str_val) == 0;
    bool aq = cst_is_seq(a), bq = cst_is_seq(b);
    if (aq || bq) {
        if (!aq || !bq) return false;
        /* list != tuple in Python */
        if (cst_is_tuple_tag(a) != cst_is_tuple_tag(b)) return false;
        size_t n = cst_seq_len(a);
        if (n != cst_seq_len(b)) return false;
        for (size_t i = 0; i < n; i++)
            if (!cst_eq(cst_seq_get(a, i), cst_seq_get(b, i))) return false;
        return true;
    }
    if (cst_is_dict(a) && cst_is_dict(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !cst_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

static json_t *cst_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *cst_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return cst_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *cst_call(cst_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = cst_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = cst_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = cst_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* ── output projection (tags -> plain JSON; mirrors the oracle canon) ── */
static json_t *cst_proj(const json_t *v) {
    if (!v) return json_null();
    if (cst_is_float_tag(v)) return json_number(json_get_num(v, "v", 0));
    if (cst_is_tuple_tag(v)) {
        json_t *a = json_array();
        const json_t *it = json_obj_get(v, "v");
        if (it && it->type == JSON_ARRAY)
            for (size_t i = 0; i < it->c.count; i++)
                json_append(a, cst_proj(json_get(it, i)));
        return a;
    }
    if (v->type == JSON_OBJECT) {
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++)
            json_set(o, v->c.keys[i], cst_proj(v->c.items[i]));
        return o;
    }
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(a, cst_proj(json_get(v, i)));
        return a;
    }
    return json_copy(v);
}

/* ── field access ── */
static const json_t *cst_fget(const json_t *obj, const char *key) {
    return cst_is_dict(obj) ? json_obj_get(obj, key) : NULL;
}
/* msg.get(key, default) — returns a BORROWED node or the supplied default. */
static const json_t *cst_get_def(const json_t *obj, const char *key, const json_t *def) {
    const json_t *v = cst_fget(obj, key);
    return v ? v : def;
}
/* x or None */
static const json_t *cst_or_none(const json_t *v) {
    return cst_truthy(v) ? v : NULL;
}

/* ── string helpers (ASCII scope) ── */
static bool cst_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v' || c == '\x1c' || c == '\x1d' || c == '\x1e' || c == '\x1f';
}
/* Python str.strip() -> malloc'd copy */
static char *cst_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && cst_is_ws(s[a])) a++;
    while (b > a && cst_is_ws(s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}
static bool cst_str_empty_after_strip(const char *s) {
    char *t = cst_strip(s);
    bool e = !t || !t[0];
    free(t);
    return e;
}
/* str.find(sub, start) -> index or -1 */
static long cst_find(const char *s, const char *sub, size_t start) {
    if (!s || !sub) return -1;
    size_t n = strlen(s);
    if (start > n) return -1;
    const char *p = strstr(s + start, sub);
    return p ? (long)(p - s) : -1;
}
/* rpartition("\n")[2] -> substring after the last '\n' (whole if none). */
static char *cst_after_last_nl(const char *s, size_t len) {
    size_t i = len;
    while (i > 0 && s[i - 1] != '\n') i--;
    char *o = malloc(len - i + 1);
    if (!o) return NULL;
    memcpy(o, s + i, len - i);
    o[len - i] = '\0';
    return o;
}
/* partition("\n")[0] -> substring before the first '\n' (whole if none). */
static char *cst_before_first_nl(const char *s, size_t len) {
    size_t i = 0;
    while (i < len && s[i] != '\n') i++;
    char *o = malloc(i + 1);
    if (!o) return NULL;
    memcpy(o, s, i);
    o[i] = '\0';
    return o;
}
static bool cst_startswith(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
/* str() of a scalar (str/int/bool/None) -> malloc'd. Domain-restricted. */
static char *cst_str_of(const json_t *v) {
    if (cst_is_str(v)) {
        char *o = malloc(strlen(v->str_val) + 1);
        if (o) strcpy(o, v->str_val);
        return o;
    }
    char buf[64];
    if (cst_is_null(v)) { strcpy(buf, "None"); }
    else if (cst_is_bool(v)) { strcpy(buf, v->bool_val ? "True" : "False"); }
    else if (cst_is_int(v)) { snprintf(buf, sizeof buf, "%.0f", v->num_val); }
    else if (cst_is_float_tag(v)) { snprintf(buf, sizeof buf, "%.17g", json_get_num(v, "v", 0)); }
    else { strcpy(buf, ""); }
    char *o = malloc(strlen(buf) + 1);
    if (o) strcpy(o, buf);
    return o;
}

/* ── list helpers ── */
/* Python list slice [start:] with negative start; end omitted (to len). */
static json_t *cst_list_slice_from(const json_t *arr, long start) {
    json_t *o = json_array();
    if (!cst_is_list(arr)) return o;
    long n = (long)arr->c.count;
    if (start < 0) start += n;
    if (start < 0) start = 0;
    if (start > n) start = n;
    for (long i = start; i < n; i++) json_append(o, json_copy(arr->c.items[i]));
    return o;
}
/* order-preserving dedup (list(dict.fromkeys(xs))) using cst_eq */
static json_t *cst_dedup(const json_t *arr) {
    json_t *o = json_array();
    if (!cst_is_list(arr)) return o;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *v = arr->c.items[i];
        bool found = false;
        for (size_t j = 0; j < o->c.count; j++)
            if (cst_eq(o->c.items[j], v)) { found = true; break; }
        if (!found) json_append(o, json_copy(v));
    }
    return o;
}

/* ── set helpers (arrays with dedup; sorted on output) ── */
static bool cst_set_contains(const json_t *set, const json_t *v) {
    if (!cst_is_list(set)) return false;
    for (size_t i = 0; i < set->c.count; i++)
        if (cst_eq(set->c.items[i], v)) return true;
    return false;
}
static void cst_set_add(json_t *set, const json_t *v) {
    if (!cst_set_contains(set, v)) json_append(set, json_copy(v));
}
static void cst_set_update(json_t *set, const json_t *other) {
    if (!cst_is_list(other)) return;
    for (size_t i = 0; i < other->c.count; i++) cst_set_add(set, other->c.items[i]);
}
static json_t *cst_set_diff(const json_t *a, const json_t *b) {
    json_t *o = json_array();
    if (!cst_is_list(a)) return o;
    for (size_t i = 0; i < a->c.count; i++)
        if (!cst_set_contains(b, a->c.items[i])) json_append(o, json_copy(a->c.items[i]));
    return o;
}
static int cst_cmp_num(const void *pa, const void *pb) {
    const json_t *a = *(const json_t *const *)pa, *b = *(const json_t *const *)pb;
    double x = cst_num(a), y = cst_num(b);
    return (x > y) - (x < y);
}
static int cst_cmp_str(const void *pa, const void *pb) {
    const json_t *a = *(const json_t *const *)pa, *b = *(const json_t *const *)pb;
    const char *x = cst_str(a), *y = cst_str(b);
    return strcmp(x ? x : "", y ? y : "");
}
/* sorted copy of a set (homogeneous int or str elements) */
static json_t *cst_set_sorted(const json_t *set) {
    json_t *o = json_array();
    if (!cst_is_list(set) || set->c.count == 0) return o;
    for (size_t i = 0; i < set->c.count; i++) json_append(o, json_copy(set->c.items[i]));
    bool numeric = cst_is_numlike(o->c.items[0]);
    qsort(o->c.items, o->c.count, sizeof(json_t *), numeric ? cst_cmp_num : cst_cmp_str);
    return o;
}

/* float(x) for the timestamp domain (int/float/None guarded by `or 0`). */
static double cst_float_of(const json_t *v) {
    return cst_is_numlike(v) ? cst_num(v) : 0.0;
}

/* ── collaborator seam wrappers (build args, call, return owned node) ── */
static json_t *cst_seam_row_identity(cst_ctx_t *ctx, const json_t *role,
                                     const json_t *content, const json_t *tcid,
                                     const json_t *tool_calls) {
    json_t *a = json_object();
    json_set(a, "role", role ? json_copy(role) : json_null());
    json_set(a, "content", content ? json_copy(content) : json_null());
    json_set(a, "tool_call_id", tcid ? json_copy(tcid) : json_null());
    json_set(a, "tool_calls", tool_calls ? json_copy(tool_calls) : json_null());
    return cst_call(ctx, "_row_identity", a);
}
static json_t *cst_seam_parse_tool_calls(cst_ctx_t *ctx, const json_t *tc) {
    json_t *a = json_object();
    json_set(a, "tool_calls", tc ? json_copy(tc) : json_null());
    return cst_call(ctx, "_parse_tool_calls", a);
}
static json_t *cst_seam_decode(cst_ctx_t *ctx, const json_t *content) {
    json_t *a = json_object();
    json_set(a, "content", content ? json_copy(content) : json_null());
    return cst_call(ctx, "_decode_content", a);
}
static json_t *cst_seam_encode(cst_ctx_t *ctx, const json_t *content) {
    json_t *a = json_object();
    json_set(a, "content", content ? json_copy(content) : json_null());
    return cst_call(ctx, "_encode_content", a);
}
static json_t *cst_seam_loaded_view(cst_ctx_t *ctx, const json_t *role, const json_t *content) {
    json_t *a = json_object();
    json_set(a, "role", role ? json_copy(role) : json_null());
    json_set(a, "content", content ? json_copy(content) : json_null());
    return cst_call(ctx, "_loaded_view_content", a);
}
static json_t *cst_seam_serialized(cst_ctx_t *ctx, const json_t *holder,
                                   const json_t *session_id, const json_t *msg, double ts) {
    json_t *a = json_object();
    json_set(a, "holder", holder ? json_copy(holder) : json_null());
    json_set(a, "session_id", session_id ? json_copy(session_id) : json_null());
    json_set(a, "msg", msg ? json_copy(msg) : json_null());
    json_t *f = json_object();
    json_set(f, "__float__", json_bool(true));
    json_set(f, "v", json_number(ts));
    json_set(a, "message_timestamp", f);
    return cst_call(ctx, "_serialized_message_row", a);
}
static json_t *cst_seam_msg_uid(cst_ctx_t *ctx, const json_t *msg) {
    json_t *a = json_object();
    json_set(a, "msg", msg ? json_copy(msg) : json_null());
    return cst_call(ctx, "message_uid_or_none", a);
}
static json_t *cst_seam_is_image(cst_ctx_t *ctx, const json_t *part) {
    json_t *a = json_object();
    json_set(a, "part", part ? json_copy(part) : json_null());
    return cst_call(ctx, "_is_image_part", a);
}
static json_t *cst_seam_coerce_epoch(cst_ctx_t *ctx, const json_t *value, const char *field) {
    json_t *a = json_object();
    json_set(a, "value", value ? json_copy(value) : json_null());
    json_set(a, "session_id", json_null());
    json_set_hdr(a, "field", field);
    return cst_call(ctx, "coerce_epoch", a);
}
static json_t *cst_seam_strip_ts(cst_ctx_t *ctx, const json_t *content) {
    json_t *a = json_object();
    json_set(a, "content", content ? json_copy(content) : json_null());
    json_set(a, "tz", json_null());
    return cst_call(ctx, "strip_leading_message_timestamps", a);
}
static json_t *cst_seam_snapshot(cst_ctx_t *ctx, const json_t *row) {
    json_t *a = json_object();
    json_set(a, "row", row ? json_copy(row) : json_null());
    return cst_call(ctx, "transcript_row_snapshot", a);
}
static json_t *cst_seam_placeholders(cst_ctx_t *ctx, const json_t *items) {
    json_t *a = json_object();
    json_set(a, "items", items ? json_copy(items) : json_null());
    return cst_call(ctx, "_placeholders", a);
}
static json_t *cst_seam_id_chunks(cst_ctx_t *ctx, const json_t *ids) {
    json_t *a = json_object();
    json_set(a, "ids", ids ? json_copy(ids) : json_null());
    json_set(a, "size", json_number(CST_SQL_IN_CHUNK));
    return cst_call(ctx, "_id_chunks", a);
}
static json_t *cst_seam_execute(cst_ctx_t *ctx, const char *sql, json_t *params) {
    json_t *a = json_object();
    json_set_hdr(a, "sql", sql);
    json_set(a, "params", params ? params : json_array());
    return cst_call(ctx, "conn.execute", a);
}

/* forward declarations of the ported case functions used as internal helpers */
static json_t *cst_index_held_dicts_node(cst_ctx_t *ctx, const json_t *holder,
                                         const json_t *compacted, json_t **by_uid_out);
static json_t *cst_claim_stored_copy_node(cst_ctx_t *ctx, const json_t *holder,
                                          const json_t *row, const json_t *compacted,
                                          const json_t *by_row, const json_t *by_uid,
                                          const json_t *claimed);
static json_t *cst_rewritten_carry_node(cst_ctx_t *ctx, const json_t *holder,
                                        const json_t *row, const json_t *row_uid,
                                        const json_t *compacted, const json_t *by_row,
                                        const json_t *by_uid, const json_t *claimed);
static json_t *cst_replays_as_node(cst_ctx_t *ctx, const json_t *holder,
                                   const json_t *row, const json_t *held, bool same_content);
static json_t *cst_rewrite_explains_node(cst_ctx_t *ctx, const json_t *holder,
                                         const json_t *row, const json_t *held);
static json_t *cst_copy_of_current_row_node(cst_ctx_t *ctx, const json_t *row, const json_t *held);
static json_t *cst_images_replaced_node(cst_ctx_t *ctx, const json_t *stored, const json_t *held);
static json_t *cst_text_of_node(const json_t *content);
static json_t *cst_holds_as_block_node(const json_t *inner, const json_t *outer);
static json_t *cst_uids_active_outside_node(cst_ctx_t *ctx, const json_t *rows,
                                            const json_t *tail_ids, const json_t *named_uids);
static json_t *cst_matching_retired_ids_node(cst_ctx_t *ctx, const json_t *holder,
                                             const json_t *session_id, const json_t *retired,
                                             const json_t *watermark);
static json_t *cst_matching_active_ids_node(cst_ctx_t *ctx, const json_t *holder,
                                            const json_t *session_id, const json_t *message);
static json_t *cst_merged_user_run_node(cst_ctx_t *ctx, const json_t *holder,
                                        const json_t *session_id, const json_t *message,
                                        const json_t *watermark);

/* ══════════════════════ pure leaves ══════════════════════ */

/* PoP: cst_text_of @ persistence/sqlite/conversation/_store/coverage.py:_text_of */
static json_t *cst_text_of_node(const json_t *content) {
    if (cst_is_str(content)) return json_copy(content);
    if (cst_is_list(content)) {
        /* "\n".join(str(part.get("text") or "") for text-parts) */
        char *buf = NULL;
        size_t cap = 0, len = 0;
        bool first = true;
        for (size_t i = 0; i < content->c.count; i++) {
            const json_t *part = content->c.items[i];
            if (!cst_is_dict(part)) continue;
            const json_t *type = cst_fget(part, "type");
            const char *ts = cst_str(type);
            if (!ts || (strcmp(ts, "text") != 0 && strcmp(ts, "input_text") != 0 &&
                        strcmp(ts, "output_text") != 0)) continue;
            const json_t *text = cst_fget(part, "text");
            const json_t *nonempty = cst_truthy(text) ? text : NULL;
            char *s = cst_str_of(nonempty ? nonempty : json_null());
            /* str(part.get("text") or ""): falsy -> "" */
            if (!nonempty) { free(s); s = malloc(1); if (s) s[0] = '\0'; }
            size_t sl = s ? strlen(s) : 0;
            size_t need = len + sl + (first ? 0 : 1) + 1;
            if (need > cap) {
                cap = need * 2 + 16;
                char *nb = realloc(buf, cap);
                if (!nb) { free(s); free(buf); return json_string(""); }
                buf = nb;
            }
            if (!first) buf[len++] = '\n';
            if (sl) { memcpy(buf + len, s, sl); len += sl; }
            first = false;
            free(s);
        }
        if (!buf) { buf = malloc(1); if (buf) buf[0] = '\0'; }
        else buf[len] = '\0';
        json_t *o = json_string(buf);
        free(buf);
        return o;
    }
    return json_string("");
}
static json_t *cst_text_of(cst_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return cst_text_of_node(cst_fget(args, "content"));
}

/* PoP: cst_holds_as_block @ persistence/sqlite/conversation/_store/coverage.py:_holds_as_block */
static json_t *cst_holds_as_block_node(const json_t *inner, const json_t *outer) {
    const char *in = cst_str(inner), *out = cst_str(outer);
    if (!in || !out) return json_bool(false);
    size_t outn = strlen(out);
    long start = cst_find(out, in, 0);
    size_t inlen = strlen(in);
    while (start != -1) {
        size_t end = (size_t)start + inlen;
        char *left = cst_after_last_nl(out, (size_t)start);
        char *right = cst_before_first_nl(out + end, outn - end);
        bool ok = cst_str_empty_after_strip(left) && cst_str_empty_after_strip(right);
        free(left);
        free(right);
        if (ok) return json_bool(true);
        start = cst_find(out, in, (size_t)start + 1);
    }
    return json_bool(false);
}
static json_t *cst_holds_as_block(cst_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return cst_holds_as_block_node(cst_fget(args, "inner"), cst_fget(args, "outer"));
}

/* PoP: cst_images_replaced @ persistence/sqlite/conversation/_store/coverage.py:_images_replaced */
static json_t *cst_images_replaced_node(cst_ctx_t *ctx, const json_t *stored, const json_t *held) {
    if (!cst_is_list(stored) || !cst_is_list(held) || stored->c.count != held->c.count)
        return json_bool(false);
    bool swapped = false;
    for (size_t i = 0; i < stored->c.count; i++) {
        const json_t *was = stored->c.items[i], *now = held->c.items[i];
        if (cst_eq(was, now)) continue;
        json_t *isimg = cst_seam_is_image(ctx, was);
        if (cst_is_raise(isimg)) return isimg;   /* BaseException passthrough */
        bool img = cst_truthy(isimg);
        json_free(isimg);
        const json_t *nowtype = cst_is_dict(now) ? cst_fget(now, "type") : NULL;
        if (!(img && cst_is_dict(now) && cst_str(nowtype) && strcmp(cst_str(nowtype), "text") == 0))
            return json_bool(false);
        swapped = true;
    }
    return json_bool(swapped);
}
static json_t *cst_images_replaced(cst_ctx_t *ctx, const json_t *args) {
    return cst_images_replaced_node(ctx, cst_fget(args, "stored"), cst_fget(args, "held"));
}

/* PoP: cst_carried_tail_slots @ persistence/sqlite/conversation/_store/coverage.py:_carried_tail_slots */
static json_t *cst_carried_tail_slots(cst_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *carried = cst_fget(args, "carried");
    double compacted_count = cst_num(cst_fget(args, "compacted_count"));
    double tail_count = cst_num(cst_fget(args, "tail_count"));
    if (tail_count <= 0) return json_number(0);
    double thresh = compacted_count - (double)(long)tail_count;
    long n = 0;
    if (cst_is_dict(carried))
        for (size_t i = 0; i < carried->c.count; i++)
            if (cst_num(carried->c.items[i]) >= thresh) n++;
    return json_number((double)n);
}

/* PoP: cst_uncounted_merged_rows @ persistence/sqlite/conversation/_store/coverage.py:_uncounted_merged_rows */
static json_t *cst_uncounted_merged_rows(cst_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *tail = cst_fget(args, "tail");
    long total = 0;
    if (!cst_is_list(tail)) return json_number(0);
    for (size_t i = 0; i < tail->c.count; i++) {
        const json_t *m = tail->c.items[i];
        if (!cst_is_dict(m)) continue;
        const json_t *rowid = cst_fget(m, "_row_id");
        if (cst_isinstance_int(rowid)) continue;   /* not isinstance(_row_id, int) */
        const json_t *merged = cst_fget(m, CST_MERGED_DURABLE);
        const json_t *unnamed = cst_fget(m, CST_UNNAMED_DURABLE);
        long unlisted = 0;
        if (cst_type_is_int(merged)) {
            const json_t *absorbed = cst_fget(m, CST_ABSORBED_ROW_IDS);
            size_t alen = cst_is_list(absorbed) ? absorbed->c.count : 0;
            unlisted = (long)cst_num(merged) - 1 - (long)alen;
        }
        long behind = (unlisted > 0 ? unlisted : 0) + (cst_type_is_int(unnamed) ? (long)cst_num(unnamed) : 0);
        total += behind;
    }
    return json_number((double)total);
}

/* PoP: cst_tail_originals @ persistence/sqlite/conversation/_store/coverage.py:_tail_originals */
static json_t *cst_tail_originals(cst_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *covered = cst_fget(args, "covered_active");
    long tail_count = (long)cst_num(cst_fget(args, "tail_count"));
    const json_t *merged_away = cst_fget(args, "merged_away");
    if (!cst_is_list(covered)) return json_array();
    long width = tail_count;
    for (;;) {
        json_t *window = cst_list_slice_from(covered, -width);
        long need = tail_count;
        for (size_t i = 0; i < window->c.count; i++)
            if (cst_set_contains(merged_away, window->c.items[i])) need++;
        if (need <= width || width >= (long)covered->c.count) return window;
        json_free(window);
        width = need;
    }
}

/* ══════════════════════ identity / claim helpers ══════════════════════ */

/* _index_held_dicts -> returns by_row (owned); *by_uid_out set to by_uid (owned).
 * by_row: {row_id(str): [index, identity, uid]}; by_uid: {uid: [[index, identity], ...]}. */
static json_t *cst_index_held_dicts_node(cst_ctx_t *ctx, const json_t *holder,
                                         const json_t *compacted, json_t **by_uid_out) {
    (void)holder;
    json_t *by_row = json_object(), *by_uid = json_object();
    json_t *unknown = json_string("unknown");
    if (cst_is_list(compacted)) {
        for (size_t index = 0; index < compacted->c.count; index++) {
            const json_t *message = compacted->c.items[index];
            if (!cst_is_dict(message)) continue;
            const json_t *role = cst_get_def(message, "role", unknown);
            const json_t *content = cst_fget(message, "content");
            const json_t *tcid = cst_fget(message, "tool_call_id");
            const json_t *rawtc = cst_fget(message, "tool_calls");
            json_t *parsed = cst_seam_parse_tool_calls(ctx, rawtc);
            if (cst_is_raise(parsed)) { json_free(by_row); json_free(by_uid); json_free(unknown); return parsed; }
            json_t *identity = cst_seam_row_identity(ctx, role, content, tcid, parsed);
            json_free(parsed);
            if (cst_is_raise(identity)) { json_free(by_row); json_free(by_uid); json_free(unknown); return identity; }
            const json_t *row_id = cst_fget(message, "_row_id");
            json_t *uid = cst_seam_msg_uid(ctx, message);
            if (cst_is_raise(uid)) { json_free(identity); json_free(by_row); json_free(by_uid); json_free(unknown); return uid; }
            if (cst_is_int(row_id) && !cst_is_bool(row_id) && cst_num(row_id) > 0) {
                char key[64];
                snprintf(key, sizeof key, "%.0f", cst_num(row_id));
                if (!json_obj_get(by_row, key)) {
                    json_t *entry = json_array();
                    json_append(entry, json_number((double)index));
                    json_append(entry, identity);   /* adopt */
                    json_append(entry, uid ? json_copy(uid) : json_null());
                    json_set(by_row, key, entry);
                    identity = NULL;
                }
            } else if (!cst_is_null(uid) && !cst_is_raise(uid)) {
                const char *u = cst_str(uid);
                if (u) {
                    json_t *lst = json_obj_get(by_uid, u);
                    if (!lst) { lst = json_array(); json_set(by_uid, u, lst); }
                    json_t *entry = json_array();
                    json_append(entry, json_number((double)index));
                    json_append(entry, identity);   /* adopt */
                    json_append(lst, entry);
                    identity = NULL;
                }
            }
            if (identity) json_free(identity);
            json_free(uid);
        }
    }
    json_free(unknown);
    *by_uid_out = by_uid;
    return by_row;
}
static json_t *cst_index_held_dicts(cst_ctx_t *ctx, const json_t *args) {
    json_t *by_uid = NULL;
    json_t *by_row = cst_index_held_dicts_node(ctx, cst_fget(args, "holder"),
                                               cst_fget(args, "compacted_messages"), &by_uid);
    if (cst_is_raise(by_row)) return by_row;
    json_t *out = json_array();
    json_append(out, cst_proj(by_row));
    json_append(out, cst_proj(by_uid));
    json_free(by_row);
    json_free(by_uid);
    return out;
}

/* by_row lookup helper: entry = [index, identity, uid] or NULL */
static const json_t *cst_byrow_get(const json_t *by_row, long row_id) {
    char key[64];
    snprintf(key, sizeof key, "%ld", row_id);
    return cst_is_dict(by_row) ? json_obj_get(by_row, key) : NULL;
}
static const json_t *cst_byuid_get(const json_t *by_uid, const char *uid) {
    if (!uid || !cst_is_dict(by_uid)) return NULL;
    return json_obj_get(by_uid, uid);
}

/* _replays_as -> bool node (or raise envelope propagated) */
static json_t *cst_replays_as_node(cst_ctx_t *ctx, const json_t *holder,
                                   const json_t *row, const json_t *held, bool same_content) {
    const json_t *session_id = cst_fget(row, "session_id");
    const json_t *tsraw = cst_fget(row, "timestamp");
    double ts = cst_float_of(cst_truthy(tsraw) ? tsraw : NULL);
    json_t *serialized = cst_seam_serialized(ctx, holder, session_id, held, ts);
    if (cst_is_raise(serialized)) return serialized;
    json_t *content = cst_seam_decode(ctx, cst_fget(row, "content"));
    if (cst_is_raise(content)) { json_free(serialized); return content; }
    const json_t *row_api_raw = cst_fget(row, "api_content");
    const json_t *ser_api = cst_fget(serialized, "api_content");
    const json_t *held_content = cst_fget(held, "content");
    /* row_api = None if api==text else api */
    const json_t *row_api = cst_eq(row_api_raw, content) ? NULL : row_api_raw;
    const json_t *held_api = cst_eq(ser_api, held_content) ? NULL : ser_api;
    bool sidecar_mismatch = false;
    for (int c = 0; c < CST_REPLAY_SIDECARS_N; c++) {
        const json_t *sv = cst_fget(serialized, CST_REPLAY_SIDECARS[c]);
        const json_t *rv = cst_fget(row, CST_REPLAY_SIDECARS[c]);
        if (!cst_is_null(sv) && !cst_eq(sv, rv)) { sidecar_mismatch = true; break; }
    }
    bool result;
    if (sidecar_mismatch) {
        result = false;
    } else if (!cst_is_null(held_api) || cst_is_null(row_api)) {
        result = cst_eq(held_api, row_api);
    } else {
        result = !same_content || cst_eq(held_content, row_api);
    }
    json_free(serialized);
    json_free(content);
    return json_bool(result);
}
static json_t *cst_replays_as(cst_ctx_t *ctx, const json_t *args) {
    const json_t *sc = cst_fget(args, "same_content");
    return cst_replays_as_node(ctx, cst_fget(args, "holder"), cst_fget(args, "row"),
                               cst_fget(args, "held"), cst_truthy(sc));
}

/* _copy_of_current_row -> bool node (or raise) */
static json_t *cst_copy_of_current_row_node(cst_ctx_t *ctx, const json_t *row, const json_t *held) {
    const json_t *snapshot = cst_fget(held, CST_DB_ROW_SNAPSHOT);
    if (!cst_is_str(snapshot)) return json_bool(false);
    json_t *snap = cst_seam_snapshot(ctx, row);
    if (cst_is_raise(snap)) return snap;
    bool eq = cst_is_str(snap) && strcmp(cst_str(snapshot), cst_str(snap)) == 0;
    json_free(snap);
    return json_bool(eq);
}
static json_t *cst_copy_of_current_row(cst_ctx_t *ctx, const json_t *args) {
    return cst_copy_of_current_row_node(ctx, cst_fget(args, "row"), cst_fget(args, "held"));
}

/* _claim_stored_copy -> int node or null (or raise) */
static json_t *cst_claim_stored_copy_node(cst_ctx_t *ctx, const json_t *holder,
                                          const json_t *row, const json_t *compacted,
                                          const json_t *by_row, const json_t *by_uid,
                                          const json_t *claimed) {
    const json_t *role = cst_fget(row, "role");
    json_t *decoded = cst_seam_decode(ctx, cst_fget(row, "content"));
    if (cst_is_raise(decoded)) return decoded;
    const json_t *tcid = cst_fget(row, "tool_call_id");
    json_t *parsed = cst_seam_parse_tool_calls(ctx, cst_fget(row, "tool_calls"));
    if (cst_is_raise(parsed)) { json_free(decoded); return parsed; }
    json_t *identity = cst_seam_row_identity(ctx, role, decoded, tcid, parsed);
    json_free(decoded);
    json_free(parsed);
    if (cst_is_raise(identity)) return identity;
    long row_id = (long)cst_num(cst_fget(row, "id"));
    /* candidates list of indices */
    json_t *candidates = json_array();
    const json_t *named = cst_byrow_get(by_row, row_id);
    if (named && cst_seq_len(named) >= 2 && cst_eq(cst_seq_get(named, 1), identity))
        json_append(candidates, json_copy(cst_seq_get(named, 0)));
    const json_t *row_uid_raw = cst_fget(row, CST_MESSAGE_UID);
    const json_t *row_uid = cst_or_none(row_uid_raw);
    if (row_uid && cst_is_str(row_uid)) {
        const json_t *lst = cst_byuid_get(by_uid, cst_str(row_uid));
        if (cst_is_list(lst))
            for (size_t i = 0; i < lst->c.count; i++) {
                const json_t *held = lst->c.items[i];
                if (cst_seq_len(held) >= 2 && cst_eq(cst_seq_get(held, 1), identity))
                    json_append(candidates, json_copy(cst_seq_get(held, 0)));
            }
    }
    json_t *result = json_null();
    for (size_t i = 0; i < candidates->c.count; i++) {
        long index = (long)cst_num(candidates->c.items[i]);
        if (cst_set_contains(claimed, candidates->c.items[i])) continue;
        const json_t *held = cst_is_list(compacted) ? json_get(compacted, (size_t)index) : NULL;
        json_t *replays = cst_replays_as_node(ctx, holder, row, held, true);
        if (cst_is_raise(replays)) { json_free(result); result = replays; break; }
        bool ok = cst_truthy(replays);
        json_free(replays);
        if (ok) { json_free(result); result = json_number((double)index); break; }
    }
    json_free(candidates);
    json_free(identity);
    return result;
}
static json_t *cst_claim_stored_copy(cst_ctx_t *ctx, const json_t *args) {
    return cst_claim_stored_copy_node(ctx, cst_fget(args, "holder"), cst_fget(args, "row"),
                                      cst_fget(args, "compacted_messages"), cst_fget(args, "by_row"),
                                      cst_fget(args, "by_uid"), cst_fget(args, "claimed"));
}

/* _rewrite_explains -> bool node (or raise) */
static json_t *cst_rewrite_explains_node(cst_ctx_t *ctx, const json_t *holder,
                                         const json_t *row, const json_t *held) {
    const json_t *role = cst_fget(row, "role");
    const json_t *held_role = cst_fget(held, "role");
    const json_t *held_tcid = cst_or_none(cst_fget(held, "tool_call_id"));
    const json_t *row_tcid = cst_or_none(cst_fget(row, "tool_call_id"));
    if (!cst_eq(held_role, role) || !cst_eq(held_tcid, row_tcid)) return json_bool(false);
    json_t *held_tc = cst_seam_parse_tool_calls(ctx, cst_fget(held, "tool_calls"));
    if (cst_is_raise(held_tc)) return held_tc;
    json_t *row_tc = cst_seam_parse_tool_calls(ctx, cst_fget(row, "tool_calls"));
    if (cst_is_raise(row_tc)) { json_free(held_tc); return row_tc; }
    const json_t *htc = cst_or_none(held_tc), *rtc = cst_or_none(row_tc);
    bool tc_diff = !cst_eq(htc, rtc);
    json_free(held_tc);
    json_free(row_tc);
    if (tc_diff) return json_bool(false);
    json_t *decoded = cst_seam_decode(ctx, cst_fget(row, "content"));
    if (cst_is_raise(decoded)) return decoded;
    json_t *stored = cst_seam_loaded_view(ctx, role, decoded);
    json_free(decoded);
    if (cst_is_raise(stored)) return stored;
    json_t *content = cst_seam_loaded_view(ctx, role, cst_fget(held, "content"));
    if (cst_is_raise(content)) { json_free(stored); return content; }
    bool same = cst_eq(stored, content);
    json_t *replays = cst_replays_as_node(ctx, holder, row, held, same);
    if (cst_is_raise(replays)) { json_free(stored); json_free(content); return replays; }
    if (!cst_truthy(replays)) { json_free(replays); json_free(stored); json_free(content); return json_bool(false); }
    json_free(replays);
    json_t *img = cst_images_replaced_node(ctx, stored, content);
    if (cst_is_raise(img)) { json_free(stored); json_free(content); return img; }
    if (cst_truthy(img)) { json_free(img); json_free(stored); json_free(content); return json_bool(true); }
    json_free(img);
    json_t *copy = cst_copy_of_current_row_node(ctx, row, held);
    if (cst_is_raise(copy)) { json_free(stored); json_free(content); return copy; }
    if (!cst_truthy(copy)) { json_free(copy); json_free(stored); json_free(content); return json_bool(false); }
    json_free(copy);
    json_t *stored_text = cst_text_of_node(stored);
    json_t *content_text = cst_text_of_node(content);
    char *stripped = cst_strip(cst_str(stored_text));
    bool nonempty = stripped && stripped[0];
    free(stripped);
    json_t *holds = cst_holds_as_block_node(stored_text, content_text);
    bool result = nonempty && cst_truthy(holds);
    json_free(holds);
    json_free(stored_text);
    json_free(content_text);
    json_free(stored);
    json_free(content);
    return json_bool(result);
}
static json_t *cst_rewrite_explains(cst_ctx_t *ctx, const json_t *args) {
    return cst_rewrite_explains_node(ctx, cst_fget(args, "holder"), cst_fget(args, "row"),
                                     cst_fget(args, "held"));
}

/* _rewritten_carry -> int node or null (or raise) */
static json_t *cst_rewritten_carry_node(cst_ctx_t *ctx, const json_t *holder,
                                        const json_t *row, const json_t *row_uid,
                                        const json_t *compacted, const json_t *by_row,
                                        const json_t *by_uid, const json_t *claimed) {
    long row_id = (long)cst_num(cst_fget(row, "id"));
    json_t *candidates = json_array();
    const json_t *named = cst_byrow_get(by_row, row_id);
    if (named && cst_seq_len(named) >= 3 && cst_eq(cst_seq_get(named, 2), row_uid)) {
        json_append(candidates, json_copy(cst_seq_get(named, 0)));
    } else {
        const char *u = cst_str(row_uid);
        const json_t *lst = cst_byuid_get(by_uid, u);
        if (cst_is_list(lst))
            for (size_t i = 0; i < lst->c.count; i++)
                json_append(candidates, json_copy(cst_seq_get(lst->c.items[i], 0)));
    }
    /* filter claimed */
    json_t *unclaimed = json_array();
    for (size_t i = 0; i < candidates->c.count; i++)
        if (!cst_set_contains(claimed, candidates->c.items[i]))
            json_append(unclaimed, json_copy(candidates->c.items[i]));
    json_free(candidates);
    json_t *result = json_null();
    if (unclaimed->c.count == 1) {
        long index = (long)cst_num(unclaimed->c.items[0]);
        const json_t *held = cst_is_list(compacted) ? json_get(compacted, (size_t)index) : NULL;
        json_t *explains = cst_rewrite_explains_node(ctx, holder, row, held);
        if (cst_is_raise(explains)) { json_free(result); result = explains; }
        else if (cst_truthy(explains)) { json_free(result); result = json_number((double)index); json_free(explains); }
        else json_free(explains);
    }
    json_free(unclaimed);
    return result;
}
static json_t *cst_rewritten_carry(cst_ctx_t *ctx, const json_t *args) {
    return cst_rewritten_carry_node(ctx, cst_fget(args, "holder"), cst_fget(args, "row"),
                                    cst_fget(args, "row_uid"), cst_fget(args, "compacted_messages"),
                                    cst_fget(args, "by_row"), cst_fget(args, "by_uid"),
                                    cst_fget(args, "claimed"));
}

/* _uids_active_outside -> set node (array) or raise */
static json_t *cst_uids_active_outside_node(cst_ctx_t *ctx, const json_t *rows,
                                            const json_t *tail_ids, const json_t *named_uids) {
    /* wanted = sorted({row[MESSAGE_UID] for row in rows if row[MESSAGE_UID] in named_uids}) */
    json_t *wanted_set = json_array();
    if (cst_is_list(rows))
        for (size_t i = 0; i < rows->c.count; i++) {
            const json_t *u = cst_fget(rows->c.items[i], CST_MESSAGE_UID);
            if (u && cst_set_contains(named_uids, u)) cst_set_add(wanted_set, u);
        }
    json_t *wanted = cst_set_sorted(wanted_set);
    json_free(wanted_set);
    json_t *result = json_array();   /* a set */
    if (wanted->c.count == 0) { json_free(wanted); return result; }
    const json_t *session_id = cst_fget(cst_is_list(rows) ? json_get(rows, 0) : NULL, "session_id");
    json_t *chunks = cst_seam_id_chunks(ctx, wanted);
    if (cst_is_raise(chunks)) { json_free(wanted); json_free(result); return chunks; }
    if (cst_is_list(chunks)) {
        for (size_t c = 0; c < chunks->c.count; c++) {
            const json_t *chunk = chunks->c.items[c];
            json_t *ph = cst_seam_placeholders(ctx, chunk);
            if (cst_is_raise(ph)) { json_free(chunks); json_free(wanted); json_free(result); return ph; }
            char sql[512];
            snprintf(sql, sizeof sql,
                     "SELECT id, %s FROM messages WHERE session_id = ? AND active = 1 AND %s IN (%s)",
                     CST_MESSAGE_UID, CST_MESSAGE_UID, cst_str(ph) ? cst_str(ph) : "");
            json_free(ph);
            json_t *params = json_array();
            json_append(params, session_id ? json_copy(session_id) : json_null());
            if (cst_is_list(chunk))
                for (size_t i = 0; i < chunk->c.count; i++) json_append(params, json_copy(chunk->c.items[i]));
            json_t *found_rows = cst_seam_execute(ctx, sql, params);
            if (cst_is_raise(found_rows)) { json_free(chunks); json_free(wanted); json_free(result); return found_rows; }
            if (cst_is_list(found_rows))
                for (size_t i = 0; i < found_rows->c.count; i++) {
                    const json_t *found = found_rows->c.items[i];
                    long fid = (long)cst_num(cst_fget(found, "id"));
                    bool in_tail = false;
                    if (cst_is_list(tail_ids))
                        for (size_t j = 0; j < tail_ids->c.count; j++)
                            if ((long)cst_num(tail_ids->c.items[j]) == fid) { in_tail = true; break; }
                    if (!in_tail) {
                        const json_t *u = cst_fget(found, CST_MESSAGE_UID);
                        if (u) cst_set_add(result, u);
                    }
                }
            json_free(found_rows);
        }
    }
    json_free(chunks);
    json_free(wanted);
    return result;
}
static json_t *cst_uids_active_outside(cst_ctx_t *ctx, const json_t *args) {
    json_t *r = cst_uids_active_outside_node(ctx, cst_fget(args, "rows"),
                                             cst_fget(args, "tail_ids"), cst_fget(args, "named_uids"));
    if (cst_is_raise(r)) return r;
    json_t *s = cst_set_sorted(r);
    json_free(r);
    return s;
}

/* _carried_tail_ids -> [carried_dict, weak_sorted] or raise */
static json_t *cst_carried_tail_ids(cst_ctx_t *ctx, const json_t *args) {
    const json_t *holder = cst_fget(args, "holder");
    const json_t *tail_ids = cst_fget(args, "tail_ids");
    const json_t *compacted = cst_fget(args, "compacted_messages");
    json_t *empty_pair = json_array();
    json_append(empty_pair, json_object());
    json_append(empty_pair, json_array());
    if (!cst_truthy(tail_ids) || !cst_truthy(compacted)) return empty_pair;
    json_free(empty_pair);
    json_t *by_uid = NULL;
    json_t *by_row = cst_index_held_dicts_node(ctx, holder, compacted, &by_uid);
    if (cst_is_raise(by_row)) return by_row;
    /* rows = conn.execute(SELECT * ... IN placeholders(tail_ids) ORDER BY id, tail_ids) */
    json_t *ph = cst_seam_placeholders(ctx, tail_ids);
    if (cst_is_raise(ph)) { json_free(by_row); json_free(by_uid); return ph; }
    char sql[512];
    snprintf(sql, sizeof sql, "SELECT * FROM messages WHERE id IN (%s) ORDER BY id",
             cst_str(ph) ? cst_str(ph) : "");
    json_free(ph);
    json_t *rows = cst_seam_execute(ctx, sql, json_copy(tail_ids));
    if (cst_is_raise(rows)) { json_free(by_row); json_free(by_uid); return rows; }
    json_t *carried = json_object();
    json_t *claimed_set = json_array();
    if (cst_is_list(rows)) {
        for (size_t i = 0; i < rows->c.count; i++) {
            const json_t *row = rows->c.items[i];
            /* claimed = set(carried.values()) */
            json_free(claimed_set);
            claimed_set = json_array();
            for (size_t k = 0; k < carried->c.count; k++) cst_set_add(claimed_set, carried->c.items[k]);
            json_t *index = cst_claim_stored_copy_node(ctx, holder, row, compacted, by_row, by_uid, claimed_set);
            if (cst_is_raise(index)) { json_free(rows); json_free(by_row); json_free(by_uid); json_free(carried); json_free(claimed_set); return index; }
            if (!cst_is_null(index)) {
                char key[64];
                snprintf(key, sizeof key, "%.0f", cst_num(cst_fget(row, "id")));
                json_set(carried, key, index);
            } else json_free(index);
        }
    }
    /* named_uids = {*by_uid, *(named[2] for named in by_row.values() if named[2] is not None)} */
    json_t *named_uids = json_array();
    for (size_t i = 0; i < by_uid->c.count; i++) {
        json_t *u = json_string(by_uid->c.keys[i]);
        cst_set_add(named_uids, u);
        json_free(u);
    }
    for (size_t i = 0; i < by_row->c.count; i++) {
        const json_t *named = by_row->c.items[i];
        const json_t *u = cst_seq_get(named, 2);
        if (!cst_is_null(u)) cst_set_add(named_uids, u);
    }
    json_t *held_elsewhere = cst_uids_active_outside_node(ctx, rows, tail_ids, named_uids);
    if (cst_is_raise(held_elsewhere)) { json_free(named_uids); json_free(rows); json_free(by_row); json_free(by_uid); json_free(carried); json_free(claimed_set); return held_elsewhere; }
    /* tail_uids = Counter(row[MESSAGE_UID] for row in rows if row[MESSAGE_UID]) */
    json_t *tail_uids = json_object();
    if (cst_is_list(rows))
        for (size_t i = 0; i < rows->c.count; i++) {
            const json_t *u = cst_fget(rows->c.items[i], CST_MESSAGE_UID);
            if (cst_truthy(u) && cst_is_str(u)) {
                double cur = json_get_num(tail_uids, cst_str(u), 0);
                json_set(tail_uids, cst_str(u), json_number(cur + 1));
            }
        }
    json_t *weak = json_array();
    if (cst_is_list(rows)) {
        for (size_t i = 0; i < rows->c.count; i++) {
            const json_t *row = rows->c.items[i];
            long row_id = (long)cst_num(cst_fget(row, "id"));
            const json_t *row_uid = cst_or_none(cst_fget(row, CST_MESSAGE_UID));
            char key[64];
            snprintf(key, sizeof key, "%ld", row_id);
            bool in_carried = json_obj_get(carried, key) != NULL;
            double cnt = row_uid && cst_is_str(row_uid) ? json_get_num(tail_uids, cst_str(row_uid), 0) : 0;
            if (in_carried || cst_is_null(row_uid) || cnt != 1 || cst_set_contains(held_elsewhere, row_uid))
                continue;
            json_free(claimed_set);
            claimed_set = json_array();
            for (size_t k = 0; k < carried->c.count; k++) cst_set_add(claimed_set, carried->c.items[k]);
            json_t *index = cst_rewritten_carry_node(ctx, holder, row, row_uid, compacted, by_row, by_uid, claimed_set);
            if (cst_is_raise(index)) { json_free(weak); json_free(tail_uids); json_free(held_elsewhere); json_free(named_uids); json_free(rows); json_free(by_row); json_free(by_uid); json_free(carried); json_free(claimed_set); return index; }
            if (!cst_is_null(index)) {
                json_set(carried, key, index);
                json_t *rid = json_number((double)row_id);
                cst_set_add(weak, rid);
                json_free(rid);
            } else json_free(index);
        }
    }
    json_t *out = json_array();
    json_append(out, cst_proj(carried));
    json_append(out, cst_set_sorted(weak));
    json_free(weak);
    json_free(tail_uids);
    json_free(held_elsewhere);
    json_free(named_uids);
    json_free(rows);
    json_free(by_row);
    json_free(by_uid);
    json_free(carried);
    json_free(claimed_set);
    return out;
}

/* ══════════════════════ DB read flows ══════════════════════ */

/* _matching_active_ids -> list[int] or raise */
static json_t *cst_matching_active_ids_node(cst_ctx_t *ctx, const json_t *holder,
                                            const json_t *session_id, const json_t *message) {
    (void)holder;
    const json_t *content = cst_fget(message, "content");
    if (!cst_is_str(content)) return json_array();
    json_t *unknown = json_string("unknown");
    const json_t *role_def = cst_get_def(message, "role", unknown);
    json_t *loaded = cst_seam_loaded_view(ctx, role_def, content);
    json_free(unknown);
    if (cst_is_raise(loaded)) return loaded;
    json_t *stored = cst_seam_encode(ctx, loaded);
    json_free(loaded);
    if (cst_is_raise(stored)) return stored;
    json_t *params = json_array();
    json_append(params, session_id ? json_copy(session_id) : json_null());
    const json_t *role = cst_fget(message, "role");
    json_append(params, role ? json_copy(role) : json_null());
    json_append(params, stored);   /* adopt */
    json_t *rows = cst_seam_execute(ctx,
        "SELECT id FROM messages WHERE session_id = ? AND active = 1 AND role = ? AND content = ?", params);
    if (cst_is_raise(rows)) return rows;
    json_t *out = json_array();
    if (cst_is_list(rows))
        for (size_t i = 0; i < rows->c.count; i++)
            json_append(out, json_number(cst_num(cst_fget(rows->c.items[i], "id"))));
    json_free(rows);
    return out;
}
static json_t *cst_matching_active_ids(cst_ctx_t *ctx, const json_t *args) {
    return cst_matching_active_ids_node(ctx, cst_fget(args, "holder"),
                                        cst_fget(args, "session_id"), cst_fget(args, "message"));
}

/* _matching_retired_ids -> list[int] or raise */
static json_t *cst_matching_retired_ids_node(cst_ctx_t *ctx, const json_t *holder,
                                             const json_t *session_id, const json_t *retired,
                                             const json_t *watermark) {
    (void)holder;
    const json_t *role_raw = cst_or_none(cst_fget(retired, "role"));
    char *role_s = cst_str_of(role_raw ? role_raw : json_null());
    /* str(retired.get("role") or "") */
    if (!role_raw) { free(role_s); role_s = malloc(1); if (role_s) role_s[0] = '\0'; }
    json_t *role_node = json_string(role_s ? role_s : "");
    bool bound = !cst_is_null(watermark);
    char sql[512];
    snprintf(sql, sizeof sql,
             "SELECT id, content, tool_call_id, tool_calls FROM messages WHERE session_id = ? AND active = 1 AND role = ?%s ORDER BY id",
             bound ? " AND id <= ?" : "");
    json_t *params = json_array();
    json_append(params, session_id ? json_copy(session_id) : json_null());
    json_append(params, json_copy(role_node));
    if (bound) json_append(params, json_number((double)(long)cst_num(watermark)));
    json_t *rows = cst_seam_execute(ctx, sql, params);
    if (cst_is_raise(rows)) { free(role_s); json_free(role_node); return rows; }
    /* want_calls = list(retired.get("tool_call_ids") or ()) */
    const json_t *want_raw = cst_or_none(cst_fget(retired, "tool_call_ids"));
    json_t *want_calls = cst_is_list(want_raw) ? json_copy(want_raw) : json_array();
    const json_t *ret_content = cst_or_none(cst_fget(retired, "content"));
    const json_t *ret_tcid = cst_or_none(cst_fget(retired, "tool_call_id"));
    json_t *matches = json_array();
    if (cst_is_list(rows)) {
        for (size_t i = 0; i < rows->c.count; i++) {
            const json_t *row = rows->c.items[i];
            json_t *decoded = cst_seam_decode(ctx, cst_fget(row, "content"));
            if (cst_is_raise(decoded)) { free(role_s); json_free(role_node); json_free(want_calls); json_free(rows); json_free(matches); return decoded; }
            json_t *content = cst_seam_loaded_view(ctx, role_node, decoded);
            json_free(decoded);
            if (cst_is_raise(content)) { free(role_s); json_free(role_node); json_free(want_calls); json_free(rows); json_free(matches); return content; }
            const json_t *content_nn = cst_or_none(content);
            bool content_diff = !cst_eq(content_nn, ret_content);
            json_free(content);
            if (content_diff) continue;
            const json_t *row_tcid = cst_or_none(cst_fget(row, "tool_call_id"));
            if (!cst_eq(row_tcid, ret_tcid)) continue;
            json_t *calls = cst_seam_parse_tool_calls(ctx, cst_fget(row, "tool_calls"));
            if (cst_is_raise(calls)) { free(role_s); json_free(role_node); json_free(want_calls); json_free(rows); json_free(matches); return calls; }
            const json_t *calls_nn = cst_or_none(calls);
            json_t *ids = json_array();
            if (cst_is_list(calls_nn))
                for (size_t j = 0; j < calls_nn->c.count; j++) {
                    const json_t *call = calls_nn->c.items[j];
                    if (cst_is_dict(call)) {
                        const json_t *id = cst_fget(call, "id");
                        json_append(ids, id ? json_copy(id) : json_null());
                    }
                }
            bool ids_diff = !cst_eq(ids, want_calls);
            json_free(ids);
            json_free(calls);
            if (ids_diff) continue;
            json_append(matches, json_number(cst_num(cst_fget(row, "id"))));
        }
    }
    free(role_s);
    json_free(role_node);
    json_free(want_calls);
    json_free(rows);
    return matches;
}
static json_t *cst_matching_retired_ids(cst_ctx_t *ctx, const json_t *args) {
    return cst_matching_retired_ids_node(ctx, cst_fget(args, "holder"),
                                         cst_fget(args, "session_id"), cst_fget(args, "retired"),
                                         cst_fget(args, "watermark"));
}

/* _resolve_carried_row_ids -> list[int] or raise.
 * by_key is a list of [key_array, rids_array] entries (composite tuple keys). */
typedef struct { json_t *by_id; json_t *by_key; } cst_index_t;
static void cst_index_free(cst_index_t *ix) {
    if (ix->by_id) json_free(ix->by_id);
    if (ix->by_key) json_free(ix->by_key);
    ix->by_id = ix->by_key = NULL;
}
static json_t *cst_resolve_index(cst_ctx_t *ctx, const json_t *session_id,
                                 const json_t *ids, cst_index_t *ix) {
    ix->by_id = json_object();
    ix->by_key = json_array();
    char narrow[512] = "";
    if (cst_truthy(ids)) {
        json_t *ph = cst_seam_placeholders(ctx, ids);
        if (cst_is_raise(ph)) return ph;
        snprintf(narrow, sizeof narrow, " AND id IN (%s)", cst_str(ph) ? cst_str(ph) : "");
        json_free(ph);
    }
    char sql[600];
    snprintf(sql, sizeof sql,
             "SELECT id, role, content, tool_call_id, tool_calls, timestamp FROM messages WHERE session_id = ? AND active = 1%s ORDER BY id",
             narrow);
    json_t *params = json_array();
    json_append(params, session_id ? json_copy(session_id) : json_null());
    if (cst_is_list(ids))
        for (size_t i = 0; i < ids->c.count; i++) json_append(params, json_copy(ids->c.items[i]));
    json_t *rows = cst_seam_execute(ctx, sql, params);
    if (cst_is_raise(rows)) return rows;
    if (cst_is_list(rows)) {
        for (size_t i = 0; i < rows->c.count; i++) {
            const json_t *row = rows->c.items[i];
            long rid = (long)cst_num(cst_fget(row, "id"));
            json_t *decoded = cst_seam_decode(ctx, cst_fget(row, "content"));
            if (cst_is_raise(decoded)) { json_free(rows); return decoded; }
            json_t *parsed = cst_seam_parse_tool_calls(ctx, cst_fget(row, "tool_calls"));
            if (cst_is_raise(parsed)) { json_free(decoded); json_free(rows); return parsed; }
            json_t *identity = cst_seam_row_identity(ctx, cst_fget(row, "role"), decoded,
                                                     cst_fget(row, "tool_call_id"), parsed);
            json_free(decoded);
            json_free(parsed);
            if (cst_is_raise(identity)) { json_free(rows); return identity; }
            char key[64];
            snprintf(key, sizeof key, "%ld", rid);
            json_set(ix->by_id, key, identity);   /* adopt */
            json_t *ts = cst_seam_coerce_epoch(ctx, cst_fget(row, "timestamp"), "message timestamp");
            if (cst_is_raise(ts)) { json_free(rows); return ts; }
            if (!cst_is_null(ts)) {
                /* composite key = (*identity, ts) */
                json_t *ck = json_array();
                for (size_t k = 0; k < cst_seq_len(identity); k++)
                    json_append(ck, json_copy(cst_seq_get(identity, k)));
                json_append(ck, json_copy(ts));
                /* find existing entry */
                json_t *rids = NULL;
                for (size_t e = 0; e < ix->by_key->c.count; e++) {
                    const json_t *entry = ix->by_key->c.items[e];
                    if (cst_eq(cst_seq_get(entry, 0), ck)) { rids = (json_t *)cst_seq_get(entry, 1); break; }
                }
                if (!rids) {
                    rids = json_array();
                    json_t *entry = json_array();
                    json_append(entry, ck);
                    json_append(entry, rids);
                    json_append(ix->by_key, entry);
                    ck = NULL;
                }
                json_append(rids, json_number((double)rid));
                if (ck) json_free(ck);
            }
            json_free(ts);
        }
    }
    json_free(rows);
    return NULL;   /* success */
}
static json_t *cst_resolve_carried_row_ids(cst_ctx_t *ctx, const json_t *args) {
    const json_t *holder = cst_fget(args, "holder");
    (void)holder;
    const json_t *session_id = cst_fget(args, "session_id");
    const json_t *carried_messages = cst_fget(args, "carried_messages");
    if (!cst_truthy(carried_messages)) return json_array();
    /* carried = [(identity, row_id, timestamp)] */
    json_t *carried = json_array();
    json_t *unknown = json_string("unknown");
    if (cst_is_list(carried_messages)) {
        for (size_t i = 0; i < carried_messages->c.count; i++) {
            const json_t *message = carried_messages->c.items[i];
            if (!cst_is_dict(message)) continue;
            const json_t *role = cst_get_def(message, "role", unknown);
            json_t *parsed = cst_seam_parse_tool_calls(ctx, cst_fget(message, "tool_calls"));
            if (cst_is_raise(parsed)) { json_free(unknown); json_free(carried); return parsed; }
            json_t *identity = cst_seam_row_identity(ctx, role, cst_fget(message, "content"),
                                                     cst_fget(message, "tool_call_id"), parsed);
            json_free(parsed);
            if (cst_is_raise(identity)) { json_free(unknown); json_free(carried); return identity; }
            const json_t *row_id_raw = cst_fget(message, "_row_id");
            json_t *row_id = json_null();
            if (cst_is_int(row_id_raw) && !cst_is_bool(row_id_raw) && cst_num(row_id_raw) > 0) {
                json_free(row_id);
                row_id = json_copy(row_id_raw);
            }
            json_t *entry = json_array();
            json_append(entry, identity);
            json_append(entry, row_id);
            const json_t *ts = cst_fget(message, "timestamp");
            json_append(entry, ts ? json_copy(ts) : json_null());
            json_append(carried, entry);
        }
    }
    json_free(unknown);
    /* row_ids = [row_id for ... if not None] */
    json_t *row_ids = json_array();
    for (size_t i = 0; i < carried->c.count; i++) {
        const json_t *rid = cst_seq_get(carried->c.items[i], 1);
        if (!cst_is_null(rid)) json_append(row_ids, json_copy(rid));
    }
    bool all_named = row_ids->c.count == carried->c.count;
    cst_index_t ix = {NULL, NULL};
    json_t *err = cst_resolve_index(ctx, session_id, all_named ? row_ids : NULL, &ix);
    if (err) { json_free(row_ids); json_free(carried); cst_index_free(&ix); return err; }
    if (all_named) {
        bool mismatch = false;
        for (size_t i = 0; i < carried->c.count; i++) {
            const json_t *ident = cst_seq_get(carried->c.items[i], 0);
            long rid = (long)cst_num(cst_seq_get(carried->c.items[i], 1));
            char key[64];
            snprintf(key, sizeof key, "%ld", rid);
            const json_t *stored = json_obj_get(ix.by_id, key);
            if (!cst_eq(stored, ident)) { mismatch = true; break; }
        }
        if (mismatch) {
            cst_index_free(&ix);
            err = cst_resolve_index(ctx, session_id, NULL, &ix);
            if (err) { json_free(row_ids); json_free(carried); cst_index_free(&ix); return err; }
        }
    }
    json_t *resolved = json_array();
    for (size_t i = 0; i < carried->c.count; i++) {
        const json_t *entry = carried->c.items[i];
        const json_t *identity = cst_seq_get(entry, 0);
        const json_t *row_id = cst_seq_get(entry, 1);
        const json_t *raw_ts = cst_seq_get(entry, 2);
        if (!cst_is_null(row_id)) {
            char key[64];
            snprintf(key, sizeof key, "%.0f", cst_num(row_id));
            const json_t *stored = json_obj_get(ix.by_id, key);
            if (cst_eq(stored, identity)) {
                json_append(resolved, json_copy(row_id));
                continue;
            }
        }
        json_t *timestamp = cst_seam_coerce_epoch(ctx, raw_ts, "message timestamp");
        if (cst_is_raise(timestamp)) { json_free(resolved); json_free(row_ids); json_free(carried); cst_index_free(&ix); return timestamp; }
        if (cst_is_null(timestamp)) { json_free(timestamp); continue; }
        /* matches = by_key.get((*identity, timestamp), []) */
        json_t *ck = json_array();
        for (size_t k = 0; k < cst_seq_len(identity); k++)
            json_append(ck, json_copy(cst_seq_get(identity, k)));
        json_append(ck, timestamp);   /* adopt */
        const json_t *matches = NULL;
        for (size_t e = 0; e < ix.by_key->c.count; e++) {
            const json_t *be = ix.by_key->c.items[e];
            if (cst_eq(cst_seq_get(be, 0), ck)) { matches = cst_seq_get(be, 1); break; }
        }
        json_free(ck);
        if (matches && cst_seq_len(matches) == 1)
            json_append(resolved, json_copy(cst_seq_get(matches, 0)));
    }
    json_t *out = cst_dedup(resolved);
    json_free(resolved);
    json_free(row_ids);
    json_free(carried);
    cst_index_free(&ix);
    return out;
}

/* _merged_user_run -> list[int] | null | raise */
static json_t *cst_merged_user_run_node(cst_ctx_t *ctx, const json_t *holder,
                                        const json_t *session_id, const json_t *message,
                                        const json_t *watermark) {
    const json_t *content_raw = cst_fget(message, "content");
    const json_t *width = cst_fget(message, CST_MERGED_DURABLE);
    const json_t *role = cst_fget(message, "role");
    if (!cst_str(role) || strcmp(cst_str(role), "user") != 0 || !cst_is_str(content_raw) ||
        !cst_type_is_int(width) || cst_num(width) < 2)
        return json_array();
    long width_n = (long)cst_num(width);
    json_t *stripped = cst_seam_strip_ts(ctx, content_raw);
    if (cst_is_raise(stripped)) return stripped;
    /* strip returns (content, epoch); take [0] */
    const json_t *content = cst_seq_get(stripped, 0);
    bool bound = !cst_is_null(watermark);
    char sql[512];
    snprintf(sql, sizeof sql,
             "SELECT id, role, content FROM messages WHERE session_id = ? AND active = 1%s ORDER BY id",
             bound ? " AND id <= ?" : "");
    json_t *params = json_array();
    json_append(params, session_id ? json_copy(session_id) : json_null());
    if (bound) json_append(params, json_number((double)(long)cst_num(watermark)));
    json_t *rows_raw = cst_seam_execute(ctx, sql, params);
    if (cst_is_raise(rows_raw)) { json_free(stripped); return rows_raw; }
    /* rows = [(id, role, loaded_view(decode(content)))] */
    json_t *rows = json_array();
    if (cst_is_list(rows_raw)) {
        for (size_t i = 0; i < rows_raw->c.count; i++) {
            const json_t *row = rows_raw->c.items[i];
            json_t *decoded = cst_seam_decode(ctx, cst_fget(row, "content"));
            if (cst_is_raise(decoded)) { json_free(rows); json_free(rows_raw); json_free(stripped); return decoded; }
            json_t *loaded = cst_seam_loaded_view(ctx, cst_fget(row, "role"), decoded);
            json_free(decoded);
            if (cst_is_raise(loaded)) { json_free(rows); json_free(rows_raw); json_free(stripped); return loaded; }
            json_t *entry = json_array();
            json_append(entry, json_number(cst_num(cst_fget(row, "id"))));
            const json_t *rrole = cst_fget(row, "role");
            json_append(entry, rrole ? json_copy(rrole) : json_null());
            json_append(entry, loaded);
            json_append(rows, entry);
        }
    }
    json_free(rows_raw);
    /* retired_ids */
    json_t *retired_ids = json_array();
    const json_t *retired_list = cst_or_none(cst_fget(message, CST_RETIRED_DURABLE));
    if (cst_is_list(retired_list)) {
        for (size_t i = 0; i < retired_list->c.count; i++) {
            const json_t *retired = retired_list->c.items[i];
            if (cst_truthy(cst_fget(retired, CST_OWN_ROW))) continue;
            json_t *matches = cst_matching_retired_ids_node(ctx, holder, session_id, retired, watermark);
            if (cst_is_raise(matches)) { json_free(retired_ids); json_free(rows); json_free(stripped); return matches; }
            if (matches->c.count != 1) {
                json_free(matches); json_free(retired_ids); json_free(rows); json_free(stripped);
                return json_null();
            }
            cst_set_update(retired_ids, matches);
            json_free(matches);
        }
    }
    /* rows = [row for row in rows if row[0] not in retired_ids] */
    json_t *filtered = json_array();
    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        if (!cst_set_contains(retired_ids, cst_seq_get(row, 0)))
            json_append(filtered, json_copy(row));
    }
    json_free(rows);
    json_free(retired_ids);
    /* runs */
    json_t *runs = json_array();
    const char *content_s = cst_str(content);
    for (size_t start = 0; start < filtered->c.count; start++) {
        char *merged = malloc(1);
        if (merged) merged[0] = '\0';
        size_t merged_len = 0;
        for (size_t end = start; end < filtered->c.count; end++) {
            const json_t *row = filtered->c.items[end];
            const json_t *rrole = cst_seq_get(row, 1);
            const json_t *part = cst_seq_get(row, 2);
            if (!cst_str(rrole) || strcmp(cst_str(rrole), "user") != 0 || !cst_is_str(part)) break;
            const char *part_s = cst_str(part);
            /* merged = f"{merged}\n\n{part}" if merged and part else (merged or part) */
            char *newmerged;
            if (merged_len > 0 && part_s[0] != '\0') {
                size_t nl = merged_len + 2 + strlen(part_s) + 1;
                newmerged = malloc(nl);
                if (newmerged) snprintf(newmerged, nl, "%s\n\n%s", merged, part_s);
            } else {
                const char *src = merged_len > 0 ? merged : part_s;
                newmerged = malloc(strlen(src) + 1);
                if (newmerged) strcpy(newmerged, src);
            }
            free(merged);
            merged = newmerged;
            merged_len = merged ? strlen(merged) : 0;
            if (!merged || !cst_startswith(content_s, merged)) break;
            if ((long)(end - start + 1) == width_n) {
                if (strcmp(merged, content_s) == 0) {
                    json_t *run = json_array();
                    for (size_t k = start; k <= end; k++)
                        json_append(run, json_copy(cst_seq_get(filtered->c.items[k], 0)));
                    json_append(runs, run);
                }
                break;
            }
        }
        free(merged);
    }
    json_free(filtered);
    json_free(stripped);
    if (runs->c.count > 1) { json_free(runs); return json_null(); }
    if (runs->c.count == 1) {
        json_t *r = json_copy(runs->c.items[0]);
        json_free(runs);
        return r;
    }
    json_free(runs);
    return json_array();
}
static json_t *cst_merged_user_run(cst_ctx_t *ctx, const json_t *args) {
    return cst_merged_user_run_node(ctx, cst_fget(args, "holder"), cst_fget(args, "session_id"),
                                    cst_fget(args, "message"), cst_fget(args, "watermark"));
}

/* _proved_coverage -> [proved_list, merged_away_sorted] | null | raise */
static json_t *cst_proved_coverage(cst_ctx_t *ctx, const json_t *args) {
    const json_t *holder = cst_fget(args, "holder");
    const json_t *session_id = cst_fget(args, "session_id");
    const json_t *covered_ids = cst_fget(args, "covered_ids");
    const json_t *unresolved = cst_fget(args, "unresolved_held");
    const json_t *watermark = cst_fget(args, "watermark");
    if (cst_is_null(covered_ids)) return json_null();
    json_t *proved = json_array();
    if (cst_is_list(covered_ids))
        for (size_t i = 0; i < covered_ids->c.count; i++) {
            const json_t *rid = covered_ids->c.items[i];
            if (cst_isinstance_int(rid) && cst_num(rid) > 0)
                json_append(proved, json_number(cst_num(rid)));
        }
    json_t *merged_away = json_array();
    const json_t *unresolved_list = cst_or_none(unresolved);
    if (cst_is_list(unresolved_list)) {
        for (size_t i = 0; i < unresolved_list->c.count; i++) {
            const json_t *message = unresolved_list->c.items[i];
            if (!cst_is_dict(message)) continue;
            if (cst_truthy(cst_fget(message, CST_RETIRED_ROW))) {
                json_t *matches = cst_matching_retired_ids_node(ctx, holder, session_id, message, watermark);
                if (cst_is_raise(matches)) { json_free(proved); json_free(merged_away); return matches; }
                if (matches->c.count != 1) { json_free(matches); json_free(proved); json_free(merged_away); return json_null(); }
                for (size_t k = 0; k < matches->c.count; k++) json_append(proved, json_copy(matches->c.items[k]));
                if (!cst_truthy(cst_fget(message, CST_OWN_ROW))) cst_set_update(merged_away, matches);
                json_free(matches);
                continue;
            }
            const json_t *retired_dur = cst_or_none(cst_fget(message, CST_RETIRED_DURABLE));
            bool has_own = false;
            if (cst_is_list(retired_dur))
                for (size_t k = 0; k < retired_dur->c.count; k++) {
                    const json_t *row = retired_dur->c.items[k];
                    if (cst_is_dict(row) && cst_truthy(cst_fget(row, CST_OWN_ROW))) { has_own = true; break; }
                }
            if (has_own) continue;
            json_t *run = cst_merged_user_run_node(ctx, holder, session_id, message, watermark);
            if (cst_is_raise(run)) { json_free(proved); json_free(merged_away); return run; }
            if (cst_truthy(cst_fget(message, CST_MERGED_DURABLE)) && !cst_truthy(run)) {
                json_free(run); json_free(proved); json_free(merged_away); return json_null();
            }
            json_t *matches;
            if (cst_truthy(run)) matches = json_array();
            else {
                matches = cst_matching_active_ids_node(ctx, holder, session_id, message);
                if (cst_is_raise(matches)) { json_free(run); json_free(proved); json_free(merged_away); return matches; }
            }
            bool abandon = matches->c.count > 1 ||
                (cst_truthy(cst_fget(message, CST_DB_PERSISTED)) && matches->c.count != 1 && !cst_truthy(run));
            if (abandon) { json_free(matches); json_free(run); json_free(proved); json_free(merged_away); return json_null(); }
            /* proved.extend(matches or run) */
            const json_t *ext = cst_truthy(matches) ? matches : run;
            if (cst_is_list(ext))
                for (size_t k = 0; k < ext->c.count; k++) json_append(proved, json_copy(ext->c.items[k]));
            /* merged_away.update(set(run[1:]) - set(absorbed)) */
            json_t *run_tail = json_array();
            if (cst_is_list(run))
                for (size_t k = 1; k < run->c.count; k++) cst_set_add(run_tail, run->c.items[k]);
            const json_t *absorbed = cst_or_none(cst_fget(message, CST_ABSORBED_ROW_IDS));
            json_t *absorbed_set = cst_is_list(absorbed) ? json_copy(absorbed) : json_array();
            json_t *diff = cst_set_diff(run_tail, absorbed_set);
            cst_set_update(merged_away, diff);
            json_free(diff);
            json_free(absorbed_set);
            json_free(run_tail);
            json_free(matches);
            json_free(run);
        }
    }
    json_t *out = json_array();
    json_append(out, cst_dedup(proved));
    json_append(out, cst_set_sorted(merged_away));
    json_free(proved);
    json_free(merged_away);
    return out;
}
