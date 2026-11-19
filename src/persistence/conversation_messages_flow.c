/*
 * conversation_messages_flow.c — transcript-contract FLOW cores of
 * persistence/conversation/messages.py (hermes-agent): the import/export
 * value helpers (_live_or_column, _count, _json_text, _same), the dataclass
 * validators (NewMessage/HeldMessage/HeldCoverage/CooldownSnapshot/
 * TranscriptRead __post_init__), the converters and projections
 * (NewMessage.from_live, ConversationMessage.as_openai_message /
 * from_openai_message, TranscriptRow.as_wire_row, RowOutcome.as_row_annotation,
 * the survivor/row-id/snapshot/as_tuple/identity/inserted accessors), the
 * stamp_live mutation flow, batch_progress, ResumeTooLarge.__init__ and the
 * ReadCheck callable gate.
 *
 * Decision sequences only: every live I/O / injected callable (json.loads in
 * _json_text, the ReadCheck _check callable) enters through ONE universal
 * seam: cm_call(ctx, name, args). The harness wires it to a recording stub fed
 * the case's scripted results (in.io); the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and cm_-prefixed.
 *
 * Representation contract (shared with tests/oracle_conversation_messages_flow.py):
 * - NULL json_t* == an absent key; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc)} object.
 * - Plain JSON numbers are Python ints; JSON bools are bools (bool IS an int
 *   instance, but type() is not int). A Python float travels as
 *   {"__float__": true, "v": <number>}; a Python tuple (where isinstance(tuple)
 *   gates read it) as {"__tuple__": true, "v": [...]}.
 * - A nested HeldMessage instance: {"__held__": true, "v": {<fields>}}; a
 *   BatchAppendProgress instance: {"__progress__": true, "v": {"outcomes":
 *   [...], "uids": [...]}}.
 * - Enum fields (RowOutcome.disposition, TranscriptRead.view,
 *   TranscriptRow.visibility) travel as the enum VALUE string; an invalid
 *   value is the live ValueError "'x' is not a valid <EnumName>".
 * - Record specs are plain field dicts; absent keys take the dataclass
 *   default; absent required / unknown keys raise the live __init__ TypeErrors.
 * - stamp_live result: null | {"kind":"progress"|"batch"|"replace", ...}; the
 *   mutated messages list is out.state.messages.
 * - batch_progress error: {"attrs": {...}} (a SimpleNamespace double) or a
 *   plain JSON value (no attributes -> getattr default None).
 * - ReadCheck check param: "seam" -> the recording stub named "check"; any
 *   other value is a non-callable attribute (TypeError on call).
 * - Dataclass-instance returns project to asdict with enums -> .value; tuples
 *   fold to arrays and floats to numbers on output (the oracle canon folds
 *   integral floats to ints on both sides).
 * - ResumeTooLarge.__init__ projects the constructed error to {"class",
 *   "message", "message_count", "limit", "scope", "is_persistence_error",
 *   "is_value_error"}; ReadCheck.__init__ to {"pinned": <value>}; the
 *   __post_init__ validators return null on success.
 * - Fixture domain: int() string literals are ASCII digits with optional +/-
 *   and single underscores between digits (plus ASCII whitespace padding);
 *   str() of non-strings covers int/bool/None only; content/metadata trees
 *   never carry literal "__float__"/"__tuple__"/"__held__"/"__progress__"
 *   keys; check-seam scripts are plain JSON values; json.loads scripts never
 *   carry __tuple__; no case combines a binding error with an invalid nested
 *   HeldMessage.
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
typedef struct cm_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} cm_ctx_t;

/* ── basic helpers ── */
static bool cm_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static bool cm_is_float_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__float__", false);
}
static bool cm_is_tuple_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__tuple__", false);
}
static bool cm_is_held_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__held__", false);
}
static bool cm_is_progress_tag(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_get_bool(v, "__progress__", false);
}
static bool cm_is_special(const json_t *v) {
    return cm_is_float_tag(v) || cm_is_tuple_tag(v) || cm_is_held_tag(v) || cm_is_progress_tag(v);
}

/* Python truthiness on the JSON representation (tags read as their value). */
static bool cm_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (cm_is_float_tag(v)) return json_get_num(v, "v", 0) != 0.0;
    if (cm_is_tuple_tag(v)) {
        const json_t *items = json_obj_get(v, "v");
        return items && items->type == JSON_ARRAY && items->c.count > 0;
    }
    if (cm_is_held_tag(v) || cm_is_progress_tag(v)) return true; /* always-truthy object */
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool cm_is_int(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool cm_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool cm_is_bool_true(const json_t *v) { return cm_is_bool(v) && v->bool_val; }
static bool cm_isinstance_int(const json_t *v) { return cm_is_int(v) || cm_is_bool(v); }
static bool cm_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool cm_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool cm_is_tuple(const json_t *v) { return cm_is_tuple_tag(v); }
static bool cm_is_list_or_tuple(const json_t *v) {
    return (v && v->type == JSON_ARRAY) || cm_is_tuple_tag(v);
}
static bool cm_is_mapping(const json_t *v) {
    return v && v->type == JSON_OBJECT && !cm_is_special(v);
}
static double cm_num(const json_t *v) {
    if (cm_is_float_tag(v)) return json_get_num(v, "v", 0);
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static const char *cm_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool cm_str_eq(const json_t *v, const char *s) {
    const char *t = cm_str(v);
    return t && s && strcmp(t, s) == 0;
}

/* Python type name for the representation (error-message domain). */
static const char *cm_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (cm_is_float_tag(v)) return "float";
    if (cm_is_tuple_tag(v)) return "tuple";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *cm_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* repr() of a Python str for error messages (single quotes unless the string
 * contains a single quote and no double quote; escape backslash/quote/ctrl). */
static char *cm_repr_str(const char *s) {
    if (!s) s = "";
    char q = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) q = '"';
    size_t cap = strlen(s) * 4 + 3, len = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    o[len++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { o[len++] = '\\'; o[len++] = (char)c; }
        else if (c == '\n') { o[len++] = '\\'; o[len++] = 'n'; }
        else if (c == '\r') { o[len++] = '\\'; o[len++] = 'r'; }
        else if (c == '\t') { o[len++] = '\\'; o[len++] = 't'; }
        else o[len++] = (char)c;
    }
    o[len++] = q;
    o[len] = '\0';
    return o;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *cm_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return cm_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return is
 * a fresh owned node (scripted value copy or raise descriptor). */
static json_t *cm_call(cm_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = cm_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = cm_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = cm_make_exc(raise);
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
static json_t *cm_proj(const json_t *v) {
    if (!v) return json_null();
    if (cm_is_float_tag(v)) return json_number(json_get_num(v, "v", 0));
    if (cm_is_tuple_tag(v)) {
        json_t *a = json_array();
        const json_t *items = json_obj_get(v, "v");
        if (items && items->type == JSON_ARRAY)
            for (size_t i = 0; i < items->c.count; i++)
                json_append(a, cm_proj(json_get(items, i)));
        return a;
    }
    if (v->type == JSON_OBJECT) {
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++)
            json_set(o, v->c.keys[i], cm_proj(v->c.items[i]));
        return o;
    }
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(a, cm_proj(json_get(v, i)));
        return a;
    }
    return json_copy(v);
}

/* field read: borrowed node or NULL (absent). */
static const json_t *cm_fget(const json_t *obj, const char *key) {
    return json_obj_get(obj, key);
}
/* owned projection of a field, null when absent. */
static json_t *cm_fproj(const json_t *obj, const char *key) {
    const json_t *v = cm_fget(obj, key);
    return v ? cm_proj(v) : json_null();
}

/* ── Python value conversions (fixture domain) ── */
static bool cm_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* int() of an ASCII digit literal with optional sign and single underscores
 * between digits, ASCII-whitespace padded. Returns false on a bad literal. */
static bool cm_parse_int_literal(const char *s, double *out) {
    while (*s && cm_ascii_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && cm_ascii_ws((unsigned char)end[-1])) end--;
    double sign = 1.0;
    if (s < end && (*s == '+' || *s == '-')) { if (*s == '-') sign = -1.0; s++; }
    double acc = 0.0;
    bool prev_digit = false, any = false;
    for (const char *p = s; p < end; p++) {
        unsigned char c = (unsigned char)*p;
        if (c >= '0' && c <= '9') { acc = acc * 10.0 + (c - '0'); prev_digit = true; any = true; }
        else if (c == '_') { if (!prev_digit) return false; prev_digit = false; }
        else return false;
    }
    if (!any || !prev_digit) return false;
    *out = sign * acc;
    return true;
}

/* int(v): a number node on success, a raise envelope on failure. */
static json_t *cm_int_val(const json_t *v) {
    if (cm_is_int(v)) return json_number(trunc(v->num_val));
    if (cm_is_bool(v)) return json_number(v->bool_val ? 1.0 : 0.0);
    if (cm_is_float_tag(v)) return json_number(trunc(cm_num(v)));
    if (cm_is_str(v)) {
        double d;
        if (cm_parse_int_literal(v->str_val, &d)) return json_number(d);
        char *r = cm_repr_str(v->str_val);
        char msg[512];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", r ? r : "''");
        free(r);
        return cm_err("ValueError", msg);
    }
    char msg[256];
    snprintf(msg, sizeof msg,
             "int() argument must be a string, a bytes-like object or a real number, not '%s'",
             cm_typename(v));
    return cm_err("TypeError", msg);
}

/* tuple(v)/list(v) iteration source: an owned JSON_ARRAY of the items, or a
 * raise envelope. Strings split into codepoints; dicts iterate their keys. */
static json_t *cm_iter_items(const json_t *v) {
    if (v && v->type == JSON_ARRAY) return json_copy(v);
    if (cm_is_tuple_tag(v)) {
        const json_t *items = json_obj_get(v, "v");
        return items ? json_copy(items) : json_array();
    }
    if (cm_is_str(v)) {
        json_t *a = json_array();
        const unsigned char *p = (const unsigned char *)v->str_val;
        while (*p) {
            size_t n = 1;
            if ((*p & 0xe0) == 0xc0) n = 2;
            else if ((*p & 0xf0) == 0xe0) n = 3;
            else if ((*p & 0xf8) == 0xf0) n = 4;
            char *buf = malloc(n + 1);
            if (buf) { memcpy(buf, p, n); buf[n] = '\0'; json_append(a, json_string(buf)); free(buf); }
            p += n;
        }
        return a;
    }
    if (cm_is_mapping(v)) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, json_string(v->c.keys[i]));
        return a;
    }
    char msg[128];
    snprintf(msg, sizeof msg, "'%s' object is not iterable", cm_typename(v));
    return cm_err("TypeError", msg);
}

/* tuple(v): an internal __tuple__ node, or a raise envelope. */
static json_t *cm_tuple_val(const json_t *v) {
    json_t *items = cm_iter_items(v);
    if (cm_is_raise(items)) return items;
    json_t *t = json_object();
    json_set(t, "__tuple__", json_bool(true));
    json_set(t, "v", items);
    return t;
}
/* list(v): an owned JSON_ARRAY, or a raise envelope. */
static json_t *cm_list_val(const json_t *v) { return cm_iter_items(v); }

/* str(v) for the int/bool/None/str domain. */
static json_t *cm_str_val(const json_t *v) {
    if (cm_is_str(v)) return json_copy(v);
    char buf[64];
    if (cm_is_bool(v)) return json_string(v->bool_val ? "True" : "False");
    if (cm_is_null(v)) return json_string("None");
    if (cm_is_int(v)) { snprintf(buf, sizeof buf, "%.0f", v->num_val); return json_string(buf); }
    if (cm_is_float_tag(v)) {
        double d = cm_num(v);
        if (d == trunc(d)) snprintf(buf, sizeof buf, "%.1f", d);
        else snprintf(buf, sizeof buf, "%.17g", d);
        return json_string(buf);
    }
    return json_string(cm_typename(v));
}

static json_t *cm_bool_val(const json_t *v) { return json_bool(cm_truthy(v)); }

/* ── dataclass binding checks (unknown kwarg first, then missing required) ── */
static bool cm_in_list(const char *key, const char *const *list, size_t n) {
    for (size_t i = 0; i < n; i++) if (strcmp(key, list[i]) == 0) return true;
    return false;
}

/* Returns NULL when the spec binds, else the live __init__ TypeError. */
static json_t *cm_binding(const json_t *spec, const char *cls,
                          const char *const *fields, size_t nfields,
                          const char *const *required, size_t nreq) {
    if (spec && spec->type == JSON_OBJECT) {
        for (size_t i = 0; i < spec->c.count; i++) {
            const char *key = spec->c.keys[i];
            if (!cm_in_list(key, fields, nfields)) {
                char msg[256];
                snprintf(msg, sizeof msg, "%s.__init__() got an unexpected keyword argument '%s'", cls, key);
                return cm_err("TypeError", msg);
            }
        }
    }
    const char *missing[40];
    size_t nm = 0;
    for (size_t i = 0; i < nreq; i++)
        if (!json_has(spec, required[i])) missing[nm++] = required[i];
    if (nm == 0) return NULL;
    char msg[1024];
    int off = snprintf(msg, sizeof msg, "%s.__init__() missing %zu required positional argument%s: ",
                       cls, nm, nm == 1 ? "" : "s");
    for (size_t i = 0; i < nm; i++) {
        if (i > 0) off += snprintf(msg + off, sizeof msg - off, nm == 2 ? " and " : (i + 1 == nm ? ", and " : ", "));
        off += snprintf(msg + off, sizeof msg - off, "'%s'", missing[i]);
    }
    return cm_err("TypeError", msg);
}

/* enum value validation: NULL when valid, else the live ValueError. */
static json_t *cm_enum(const json_t *v, const char *const *vals, size_t n, const char *ename) {
    const char *s = cm_str(v);
    if (s && cm_in_list(s, vals, n)) return NULL;
    char *r = cm_repr_str(s ? s : "");
    char msg[256];
    snprintf(msg, sizeof msg, "%s is not a valid %s", r ? r : "''", ename);
    free(r);
    return cm_err("ValueError", msg);
}

/* ── field tables ── */
static const char *const NM_FIELDS[] = {
    "role", "content", "timestamp", "tool_calls", "tool_name", "tool_call_id", "display_kind",
    "compressed_summary", "api_content", "display_metadata", "platform_message_id", "observed",
    "effect_disposition", "finish_reason", "reasoning", "reasoning_content", "reasoning_details",
    "codex_reasoning_items", "codex_message_items", "repair_target", "db_row_snapshot", "message_uid",
    "absorbed_message_uids", "tool_call_uids", "tool_call_uid", "token_count", "data_use",
    "merged_durable_rows", "unnamed_durable_rows", "absorbed_row_ids", "restored_from_store"};
static const char *const NM_REQ[] = {"role", "content"};
static const char *const HM_FIELDS[] = {
    "role", "content", "tool_call_id", "tool_calls", "timestamp", "row_id", "persisted",
    "db_row_snapshot", "message_uid", "absorbed_row_ids", "merged_durable_rows",
    "retired_durable_rows", "tool_call_ids", "own_row", "retired_row"};
static const char *const HM_REQ[] = {"role", "content"};
static const char *const RS_FIELDS[] = {"id", "timestamp", "db_row_snapshot", "message_uid", "tool_call_uids", "tool_call_uid"};
static const char *const RS_REQ[] = {"id", "timestamp", "db_row_snapshot", "message_uid"};
static const char *const RO_FIELDS[] = {"id", "disposition", "timestamp", "db_row_snapshot", "canonical_row", "message_uid", "tool_call_uids", "tool_call_uid"};
static const char *const RO_REQ[] = {"id", "disposition", "timestamp", "db_row_snapshot"};
static const char *const RU_FIELDS[] = {"message_uid", "tool_call_uids", "tool_call_uid"};
static const char *const CD_FIELDS[] = {"present", "until", "error"};
static const char *const CD_REQ[] = {"present"};
static const char *const CV_FIELDS[] = {"lineage_ids", "active_count", "max_active_id", "transcript_revision"};
static const char *const CV_REQ[] = {"lineage_ids", "active_count", "max_active_id", "transcript_revision"};
static const char *const DD_FIELDS[] = {"content", "metadata"};
static const char *const DD_REQ[] = {"content", "metadata"};
static const char *const HC_FIELDS[] = {"covered_ids", "unresolved"};
static const char *const TR_FIELDS[] = {"view", "limit", "offset", "latest", "after_id", "include_lineage"};
static const char *const TW_FIELDS[] = {
    "id", "session_id", "role", "content", "timestamp", "visibility", "tool_call_id", "tool_calls",
    "tool_name", "effect_disposition", "token_count", "finish_reason", "reasoning", "reasoning_content",
    "reasoning_details", "codex_reasoning_items", "codex_message_items", "platform_message_id",
    "observed", "compressed_summary", "api_content", "display_kind", "display_metadata", "message_uid",
    "absorbed_message_uids", "tool_call_uids", "tool_call_uid"};
static const char *const TW_REQ[] = {"id", "session_id", "role", "content", "timestamp", "visibility"};
static const char *const BAR_FIELDS[] = {"outcomes"};
static const char *const BAR_REQ[] = {"outcomes"};
static const char *const BAP_FIELDS[] = {"outcomes", "uids"};
static const char *const BAP_REQ[] = {"outcomes", "uids"};
static const char *const RR_FIELDS[] = {"rows"};
static const char *const RR_REQ[] = {"rows"};
static const char *const CR_FIELDS[] = {"active_count", "rows"};
static const char *const CR_REQ[] = {"active_count", "rows"};
static const char *const CM_FIELDS[] = {
    "role", "content", "db_row_snapshot", "row_id", "message_uid", "tool_call_uids", "tool_call_uid",
    "absorbed_message_uids", "api_content", "display_kind", "display_metadata", "data_use",
    "compressed_summary", "timestamp", "tool_call_id", "tool_name", "effect_disposition", "tool_calls",
    "message_id", "observed", "finish_reason", "reasoning", "reasoning_content", "reasoning_details",
    "codex_reasoning_items", "codex_message_items", "absorbed_row_ids", "merged_turn_prefix",
    "merged_durable_rows", "unnamed_durable_rows", "retired_durable_rows", "db_persisted",
    "_tool_call_uid_stored"};
static const char *const CM_REQ[] = {"role", "content"};

static const char *const ROW_DISPOSITION[] = {"appended", "repaired", "adopted"};
static const char *const TRANSCRIPT_VIEW[] = {"active", "display", "all"};
static const char *const ROW_VISIBILITY[] = {"active", "rewound", "compaction_archived"};

/* ── record validators (materialization) ── */
static json_t *cm_row_stamp_check(const json_t *spec) {
    return cm_binding(spec, "RowStamp", RS_FIELDS, 6, RS_REQ, 4);
}
static json_t *cm_row_uids_check(const json_t *spec) {
    return cm_binding(spec, "RowUids", RU_FIELDS, 3, NULL, 0);
}
static json_t *cm_row_outcome_check(const json_t *spec) {
    json_t *e = cm_binding(spec, "RowOutcome", RO_FIELDS, 8, RO_REQ, 4);
    if (e) return e;
    const json_t *d = cm_fget(spec, "disposition");
    if (d) return cm_enum(d, ROW_DISPOSITION, 3, "RowDisposition");
    return NULL;
}
static json_t *cm_progress_check(const json_t *spec) {
    json_t *e = cm_binding(spec, "BatchAppendProgress", BAP_FIELDS, 2, BAP_REQ, 2);
    if (e) return e;
    const json_t *oc = cm_fget(spec, "outcomes");
    if (oc && oc->type == JSON_ARRAY)
        for (size_t i = 0; i < oc->c.count; i++) {
            e = cm_row_outcome_check(json_get(oc, i));
            if (e) return e;
        }
    const json_t *ui = cm_fget(spec, "uids");
    if (ui && ui->type == JSON_ARRAY)
        for (size_t i = 0; i < ui->c.count; i++) {
            e = cm_row_uids_check(json_get(ui, i));
            if (e) return e;
        }
    return NULL;
}

/* forward decl for nested held validation */
static json_t *cm_held_check(const json_t *spec);

/* Materialize __held__ elements of a retired/unresolved sequence NOW (their
 * own validation runs before the outer construction). Non-tag elements are
 * left for the outer isinstance gate. */
static json_t *cm_held_seq_mat(const json_t *seq) {
    if (!cm_is_list_or_tuple(seq)) return NULL;
    const json_t *items = cm_is_tuple_tag(seq) ? json_obj_get(seq, "v") : seq;
    if (!items || items->type != JSON_ARRAY) return NULL;
    for (size_t i = 0; i < items->c.count; i++) {
        const json_t *e = json_get(items, i);
        if (cm_is_held_tag(e)) {
            json_t *err = cm_held_check(json_obj_get(e, "v"));
            if (err) return err;
        }
    }
    return NULL;
}

static json_t *cm_held_check(const json_t *spec) {
    json_t *e = cm_held_seq_mat(cm_fget(spec, "retired_durable_rows"));
    if (e) return e;
    e = cm_binding(spec, "HeldMessage", HM_FIELDS, 15, HM_REQ, 2);
    if (e) return e;
    const json_t *v;
    v = cm_fget(spec, "role");
    if (!cm_is_str(v) || !cm_truthy(v)) return cm_err("ValueError", "HeldMessage.role must be a non-empty string");
    v = cm_fget(spec, "tool_call_id");
    if (!cm_is_null(v) && !cm_is_str(v)) return cm_err("ValueError", "HeldMessage.tool_call_id must be a string or None");
    v = cm_fget(spec, "tool_calls");
    if (v && !cm_is_tuple(v)) return cm_err("ValueError", "HeldMessage.tool_calls must be a tuple");
    v = cm_fget(spec, "timestamp");
    if (!cm_is_null(v) && !(cm_is_int(v) || cm_is_float_tag(v)))
        return cm_err("ValueError", "HeldMessage.timestamp must be an epoch number or None");
    v = cm_fget(spec, "row_id");
    if (!cm_is_null(v) && !(cm_is_int(v) && cm_num(v) > 0))
        return cm_err("ValueError", "HeldMessage.row_id must be a positive MessageId or None");
    v = cm_fget(spec, "persisted");
    if (v && !cm_is_bool(v)) return cm_err("ValueError", "HeldMessage.persisted must be a bool");
    v = cm_fget(spec, "merged_durable_rows");
    if (v && !(cm_is_int(v) && cm_num(v) >= 0))
        return cm_err("ValueError", "HeldMessage.merged_durable_rows must be a non-negative int");
    v = cm_fget(spec, "retired_durable_rows");
    if (v) {
        if (!cm_is_tuple(v)) return cm_err("ValueError", "HeldMessage.retired_durable_rows must be a tuple of HeldMessage");
        const json_t *items = json_obj_get(v, "v");
        if (items && items->type == JSON_ARRAY)
            for (size_t i = 0; i < items->c.count; i++)
                if (!cm_is_held_tag(json_get(items, i)))
                    return cm_err("ValueError", "HeldMessage.retired_durable_rows must be a tuple of HeldMessage");
    }
    v = cm_fget(spec, "tool_call_ids");
    if (v && !cm_is_tuple(v)) return cm_err("ValueError", "HeldMessage.tool_call_ids must be a tuple");
    v = cm_fget(spec, "own_row");
    if (v && !cm_is_bool(v)) return cm_err("ValueError", "HeldMessage.own_row and retired_row must be bools");
    v = cm_fget(spec, "retired_row");
    if (v && !cm_is_bool(v)) return cm_err("ValueError", "HeldMessage.own_row and retired_row must be bools");
    return NULL;
}

static json_t *cm_coverage_check(const json_t *spec) {
    json_t *e = cm_held_seq_mat(cm_fget(spec, "unresolved"));
    if (e) return e;
    e = cm_binding(spec, "HeldCoverage", HC_FIELDS, 2, NULL, 0);
    if (e) return e;
    const json_t *v = cm_fget(spec, "covered_ids");
    if (v) {
        bool ok = cm_is_tuple(v);
        const json_t *items = ok ? json_obj_get(v, "v") : NULL;
        if (ok && items && items->type == JSON_ARRAY)
            for (size_t i = 0; i < items->c.count; i++) {
                const json_t *x = json_get(items, i);
                if (!(cm_is_int(x) && cm_num(x) > 0)) { ok = false; break; }
            }
        if (!ok) return cm_err("ValueError", "HeldCoverage.covered_ids must be a tuple of positive MessageIds");
    }
    v = cm_fget(spec, "unresolved");
    if (v) {
        bool ok = cm_is_tuple(v);
        const json_t *items = ok ? json_obj_get(v, "v") : NULL;
        if (ok && items && items->type == JSON_ARRAY)
            for (size_t i = 0; i < items->c.count; i++)
                if (!cm_is_held_tag(json_get(items, i))) { ok = false; break; }
        if (!ok) return cm_err("ValueError", "HeldCoverage.unresolved must be a tuple of HeldMessage");
    }
    return NULL;
}

static json_t *cm_new_message_check(const json_t *spec) {
    json_t *e = cm_binding(spec, "NewMessage", NM_FIELDS, 31, NM_REQ, 2);
    if (e) return e;
    if (!cm_truthy(cm_fget(spec, "role"))) return cm_err("ValueError", "NewMessage.role must be non-empty");
    const json_t *m = cm_fget(spec, "merged_durable_rows");
    if (m && !(cm_is_int(m) && cm_num(m) >= 0))
        return cm_err("ValueError", "NewMessage.merged_durable_rows and unnamed_durable_rows must be non-negative ints");
    const json_t *u = cm_fget(spec, "unnamed_durable_rows");
    if (u && !(cm_is_int(u) && cm_num(u) >= 0))
        return cm_err("ValueError", "NewMessage.merged_durable_rows and unnamed_durable_rows must be non-negative ints");
    const json_t *a = cm_fget(spec, "absorbed_row_ids");
    if (a) {
        bool ok = cm_is_tuple(a);
        const json_t *items = ok ? json_obj_get(a, "v") : NULL;
        if (ok && items && items->type == JSON_ARRAY)
            for (size_t i = 0; i < items->c.count; i++) {
                const json_t *x = json_get(items, i);
                if (!(cm_is_int(x) && cm_num(x) > 0)) { ok = false; break; }
            }
        if (!ok) return cm_err("ValueError", "NewMessage.absorbed_row_ids must be a tuple of positive MessageIds");
    }
    return NULL;
}

/* ── shared projections ── */
static json_t *cm_row_outcome_proj(const json_t *o) {
    json_t *d = json_object();
    json_set(d, "id", cm_fproj(o, "id"));
    json_set(d, "disposition", cm_fproj(o, "disposition"));
    json_set(d, "timestamp", cm_fproj(o, "timestamp"));
    json_set(d, "db_row_snapshot", cm_fproj(o, "db_row_snapshot"));
    json_set(d, "canonical_row", cm_fproj(o, "canonical_row"));
    json_set(d, "message_uid", cm_fproj(o, "message_uid"));
    json_set(d, "tool_call_uids", cm_fproj(o, "tool_call_uids"));
    json_set(d, "tool_call_uid", cm_fproj(o, "tool_call_uid"));
    return d;
}
static json_t *cm_row_uids_proj(const json_t *u) {
    json_t *d = json_object();
    json_set(d, "message_uid", cm_fproj(u, "message_uid"));
    json_set(d, "tool_call_uids", cm_fproj(u, "tool_call_uids"));
    json_set(d, "tool_call_uid", cm_fproj(u, "tool_call_uid"));
    return d;
}
static json_t *cm_progress_proj(const json_t *p) {
    json_t *d = json_object();
    json_t *oc = json_array();
    const json_t *outcomes = cm_fget(p, "outcomes");
    if (outcomes && outcomes->type == JSON_ARRAY)
        for (size_t i = 0; i < outcomes->c.count; i++) json_append(oc, cm_row_outcome_proj(json_get(outcomes, i)));
    json_set(d, "outcomes", oc);
    json_t *ui = json_array();
    const json_t *uids = cm_fget(p, "uids");
    if (uids && uids->type == JSON_ARRAY)
        for (size_t i = 0; i < uids->c.count; i++) json_append(ui, cm_row_uids_proj(json_get(uids, i)));
    json_set(d, "uids", ui);
    return d;
}

/* RowOutcome.as_row_annotation() dict (spec already validated). Owned dict or
 * a raise envelope from int(id). */
static json_t *cm_row_outcome_annotation(const json_t *o) {
    json_t *id = cm_int_val(cm_fget(o, "id"));
    if (cm_is_raise(id)) return id;
    json_t *d = json_object();
    json_set(d, "_row_id", id);
    json_set(d, "timestamp", cm_fproj(o, "timestamp"));
    json_set(d, "_db_row_snapshot", cm_fproj(o, "db_row_snapshot"));
    json_set(d, "_canonical_row", cm_fproj(o, "canonical_row"));
    json_set(d, "message_uid", cm_fproj(o, "message_uid"));
    const json_t *tcu = cm_fget(o, "tool_call_uids");
    json_set(d, "_tool_call_uids", cm_is_null(tcu) ? json_null() : cm_proj(tcu));
    json_set(d, "_tool_call_uid", cm_fproj(o, "tool_call_uid"));
    return d;
}

/* ── _json_text / _live_or_column internal nodes ── */
static const json_t *cm_live_or_column_node(const json_t *msg, const char *live_key, const char *column) {
    if (json_has(msg, live_key)) return json_obj_get(msg, live_key);
    return json_obj_get(msg, column);
}

/* _json_text returning an owned node (raise envelope on a non-ValueError). */
static json_t *cm_json_text_node(cm_ctx_t *ctx, const json_t *value, const json_t *empty) {
    if (!cm_is_str(value)) return value ? json_copy(value) : json_null();
    json_t *a = json_object();
    json_set(a, "s", json_copy(value));
    json_t *ret = cm_call(ctx, "json.loads", a);
    if (cm_is_raise(ret)) {
        if (strcmp(json_get_str(ret, "__raise__", ""), "ValueError") == 0) {
            json_free(ret);
            return empty ? json_copy(empty) : json_null();
        }
        return ret;
    }
    return ret;
}

static json_t *cm_count_node(const json_t *v) {
    if (cm_is_int(v) && cm_num(v) > 0) return json_number(cm_num(v));
    return json_number(0);
}

/* ══════════════════════ case functions ══════════════════════ */

/* PoP: cm_live_or_column @ persistence/conversation/messages.py:_live_or_column */
static json_t *cm_live_or_column(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *msg = cm_fget(args, "message");
    const char *lk = json_get_str(args, "live_key", "");
    const char *col = json_get_str(args, "column", "");
    const json_t *v = cm_live_or_column_node(msg, lk, col);
    return v ? cm_proj(v) : json_null();
}

/* PoP: cm_count @ persistence/conversation/messages.py:_count */
static json_t *cm_count(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return cm_count_node(cm_fget(args, "value"));
}

/* PoP: cm_json_text @ persistence/conversation/messages.py:_json_text */
static json_t *cm_json_text(cm_ctx_t *ctx, const json_t *args) {
    json_t *r = cm_json_text_node(ctx, cm_fget(args, "value"), cm_fget(args, "empty"));
    if (cm_is_raise(r)) return r;
    json_t *p = cm_proj(r);
    json_free(r);
    return p;
}

/* PoP: cm_same @ persistence/conversation/messages.py:_same */
static json_t *cm_same(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return cm_proj(cm_fget(args, "value"));
}

/* PoP: cm_new_message_post_init @ persistence/conversation/messages.py:NewMessage.__post_init__ */
static json_t *cm_new_message_post_init(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *e = cm_new_message_check(cm_fget(args, "fields"));
    return e ? e : json_null();
}

/* PoP: cm_new_message_from_live @ persistence/conversation/messages.py:NewMessage.from_live */
static json_t *cm_new_message_from_live(cm_ctx_t *ctx, const json_t *args) {
    const json_t *msg = cm_fget(args, "message");
    json_t *out = json_object();
    const json_t *v;
    json_t *tmp;
#define FGET(k) cm_fget(msg, k)
#define FPROJ(k) cm_fproj(msg, k)
    /* role (default "unknown" only when absent) */
    v = FGET("role"); json_set(out, "role", v ? cm_proj(v) : json_string("unknown"));
    json_set(out, "content", FPROJ("content"));
    json_set(out, "timestamp", FPROJ("timestamp"));
    /* tool_calls = tuple(_json_text(get("tool_calls"), [])) when list/tuple else () */
    tmp = cm_json_text_node(ctx, FGET("tool_calls"), json_array());
    if (cm_is_raise(tmp)) { json_free(out); return tmp; }
    json_set(out, "tool_calls", cm_is_list_or_tuple(tmp) ? cm_proj(tmp) : json_array());
    json_free(tmp);
    json_set(out, "tool_name", FPROJ("tool_name"));
    json_set(out, "tool_call_id", FPROJ("tool_call_id"));
    json_set(out, "display_kind", FPROJ("display_kind"));
    json_set(out, "compressed_summary", cm_bool_val(FGET("_compressed_summary")));
    json_set(out, "api_content", FPROJ("api_content"));
    json_set(out, "display_metadata", FPROJ("display_metadata"));
    /* platform_message_id = get("platform_message_id") or get("message_id") or None */
    v = FGET("platform_message_id");
    if (cm_truthy(v)) json_set(out, "platform_message_id", cm_proj(v));
    else { v = FGET("message_id"); json_set(out, "platform_message_id", cm_truthy(v) ? cm_proj(v) : json_null()); }
    json_set(out, "observed", cm_bool_val(FGET("observed")));
    json_set(out, "effect_disposition", FPROJ("effect_disposition"));
    json_set(out, "finish_reason", FPROJ("finish_reason"));
    json_set(out, "reasoning", FPROJ("reasoning"));
    json_set(out, "reasoning_content", FPROJ("reasoning_content"));
    json_set(out, "reasoning_details", FPROJ("reasoning_details"));
    json_set(out, "codex_reasoning_items", FPROJ("codex_reasoning_items"));
    json_set(out, "codex_message_items", FPROJ("codex_message_items"));
    /* repair_target = MessageId(row_id) when isinstance int and not bool */
    v = FGET("_row_id");
    json_set(out, "repair_target", (cm_isinstance_int(v) && !cm_is_bool(v)) ? cm_proj(v) : json_null());
    v = FGET("_db_row_snapshot");
    json_set(out, "db_row_snapshot", cm_is_str(v) ? cm_proj(v) : json_null());
    v = FGET("message_uid");
    json_set(out, "message_uid", (cm_is_str(v) && cm_truthy(v)) ? cm_proj(v) : json_null());
    /* absorbed_message_uids */
    tmp = cm_json_text_node(ctx, cm_live_or_column_node(msg, "_absorbed_message_uids", "absorbed_message_uids"), json_array());
    if (cm_is_raise(tmp)) { json_free(out); return tmp; }
    json_set(out, "absorbed_message_uids", cm_is_list_or_tuple(tmp) ? cm_proj(tmp) : json_array());
    json_free(tmp);
    /* tool_call_uids (Mapping gate) */
    tmp = cm_json_text_node(ctx, cm_live_or_column_node(msg, "_tool_call_uids", "tool_call_uids"), json_object());
    if (cm_is_raise(tmp)) { json_free(out); return tmp; }
    json_set(out, "tool_call_uids", cm_is_mapping(tmp) ? cm_proj(tmp) : json_null());
    json_free(tmp);
    v = cm_live_or_column_node(msg, "_tool_call_uid", "tool_call_uid");
    json_set(out, "tool_call_uid", (cm_is_str(v) && cm_truthy(v)) ? cm_proj(v) : json_null());
    json_set(out, "token_count", FPROJ("token_count"));
    /* data_use = tuple(get("data_use") or ()) */
    v = FGET("data_use");
    if (cm_truthy(v)) {
        json_t *t = cm_tuple_val(v);
        if (cm_is_raise(t)) { json_free(out); return t; }
        json_set(out, "data_use", cm_proj(t));
        json_free(t);
    } else json_set(out, "data_use", json_array());
    json_set(out, "merged_durable_rows", cm_count_node(FGET("_merged_durable_rows")));
    json_set(out, "unnamed_durable_rows", cm_count_node(FGET("_unnamed_durable_rows")));
    /* absorbed_row_ids = tuple(i for i in ids if _count(i)) when list/tuple */
    json_t *aids = json_array();
    v = FGET("_absorbed_row_ids");
    if (cm_is_list_or_tuple(v)) {
        const json_t *items = cm_is_tuple_tag(v) ? json_obj_get(v, "v") : v;
        if (items && items->type == JSON_ARRAY)
            for (size_t i = 0; i < items->c.count; i++) {
                const json_t *x = json_get(items, i);
                if (cm_is_int(x) && cm_num(x) > 0) json_append(aids, cm_proj(x));
            }
    }
    json_set(out, "absorbed_row_ids", aids);
    v = FGET("_restored_from_store");
    json_set(out, "restored_from_store", json_bool(cm_is_bool_true(v)));
#undef FGET
#undef FPROJ
    /* cls(...) runs __post_init__: only role can fail here (the markers are
     * always well-typed by construction). */
    if (!cm_truthy(json_obj_get(out, "role"))) {
        json_free(out);
        return cm_err("ValueError", "NewMessage.role must be non-empty");
    }
    return out;
}

/* PoP: cm_row_outcome_as_row_annotation @ persistence/conversation/messages.py:RowOutcome.as_row_annotation */
static json_t *cm_row_outcome_as_row_annotation(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *spec = cm_fget(args, "outcome");
    json_t *e = cm_row_outcome_check(spec);
    if (e) return e;
    return cm_row_outcome_annotation(spec);
}

/* PoP: cm_batch_append_result_inserted @ persistence/conversation/messages.py:BatchAppendResult.inserted */
static json_t *cm_batch_append_result_inserted(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *e = cm_binding(args, "BatchAppendResult", BAR_FIELDS, 1, BAR_REQ, 1);
    if (e) return e;
    const json_t *oc = cm_fget(args, "outcomes");
    double count = 0;
    if (oc && oc->type == JSON_ARRAY)
        for (size_t i = 0; i < oc->c.count; i++) {
            const json_t *o = json_get(oc, i);
            e = cm_row_outcome_check(o);
            if (e) return e;
            if (cm_str_eq(cm_fget(o, "disposition"), "appended")) count += 1;
        }
    return json_number(count);
}

/* PoP: cm_batch_progress @ persistence/conversation/messages.py:batch_progress */
static json_t *cm_batch_progress(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *espec = cm_fget(args, "error");
    if (cm_is_mapping(espec) && json_has(espec, "attrs")) {
        const json_t *attrs = cm_fget(espec, "attrs");
        const json_t *bp = cm_fget(attrs, "batch_progress");
        if (cm_is_progress_tag(bp)) {
            const json_t *v = cm_fget(bp, "v");
            json_t *e = cm_progress_check(v);
            if (e) return e;
            return cm_progress_proj(v);
        }
    }
    return json_null();
}

/* PoP: cm_held_message_post_init @ persistence/conversation/messages.py:HeldMessage.__post_init__ */
static json_t *cm_held_message_post_init(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *e = cm_held_check(cm_fget(args, "fields"));
    return e ? e : json_null();
}

/* PoP: cm_held_coverage_post_init @ persistence/conversation/messages.py:HeldCoverage.__post_init__ */
static json_t *cm_held_coverage_post_init(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *e = cm_coverage_check(cm_fget(args, "fields"));
    return e ? e : json_null();
}

/* PoP: cm_replace_result_survivor_ids @ persistence/conversation/messages.py:ReplaceResult.survivor_ids */
static json_t *cm_replace_result_survivor_ids(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *e = cm_binding(args, "ReplaceResult", RR_FIELDS, 1, RR_REQ, 1);
    if (e) return e;
    json_t *out = json_array();
    const json_t *rows = cm_fget(args, "rows");
    if (rows && rows->type == JSON_ARRAY)
        for (size_t i = 0; i < rows->c.count; i++) {
            const json_t *r = json_get(rows, i);
            e = cm_row_stamp_check(r);
            if (e) { json_free(out); return e; }
            json_append(out, cm_fproj(r, "id"));
        }
    return out;
}

/* stamp_live batch rung: stamp each message from its outcome annotation.
 * Mutates msgs in place; returns a raise envelope (partial state kept) or NULL. */
static json_t *cm_stamp_batch(json_t *msgs, const json_t *outcomes) {
    size_t n = json_len(msgs);
    size_t m = (outcomes && outcomes->type == JSON_ARRAY) ? outcomes->c.count : 0;
    for (size_t i = 0; i < n && i < m; i++) {
        json_t *ann = cm_row_outcome_annotation(json_get(outcomes, i));
        if (cm_is_raise(ann)) return ann;
        json_t *msg = json_get(msgs, i);
        for (size_t k = 0; k < ann->c.count; k++) {
            json_t *val = ann->c.items[k];
            if (!cm_is_null(val)) json_set(msg, ann->c.keys[k], json_copy(val));
        }
        json_free(ann);
    }
    return NULL;
}

/* stamp_live replace rung. */
static json_t *cm_stamp_replace(json_t *msgs, const json_t *rows) {
    size_t n = json_len(msgs);
    size_t m = (rows && rows->type == JSON_ARRAY) ? rows->c.count : 0;
    for (size_t i = 0; i < n && i < m; i++) {
        const json_t *r = json_get(rows, i);
        json_t *msg = json_get(msgs, i);
        json_t *id = cm_int_val(cm_fget(r, "id"));
        if (cm_is_raise(id)) return id;
        json_set(msg, "_row_id", id);
        const json_t *uid = cm_fget(r, "message_uid");
        if (cm_truthy(uid)) json_set(msg, "message_uid", cm_proj(uid));
        const json_t *snap = cm_fget(r, "db_row_snapshot");
        if (cm_is_null(snap)) continue;
        json_set(msg, "timestamp", cm_fproj(r, "timestamp"));
        json_set(msg, "_db_row_snapshot", cm_proj(snap));
        const json_t *tcu = cm_fget(r, "tool_call_uids");
        if (!cm_is_null(tcu)) json_set(msg, "_tool_call_uids", cm_proj(tcu));
        const json_t *tcuid = cm_fget(r, "tool_call_uid");
        if (!cm_is_null(tcuid)) json_set(msg, "_tool_call_uid", cm_proj(tcuid));
    }
    return NULL;
}

/* PoP: cm_stamp_live @ persistence/conversation/messages.py:stamp_live */
static json_t *cm_stamp_live(cm_ctx_t *ctx, const json_t *args) {
    const json_t *messages = cm_fget(args, "messages");
    const json_t *result = cm_fget(args, "result");
    const char *kind = cm_is_mapping(result) ? cm_str(cm_fget(result, "kind")) : NULL;
    /* Strip the harness-only "kind" tag for record validation: live mat_result
     * constructs the dataclass from the body WITHOUT it, so "kind" is not an
     * unknown kwarg. The copy stays alive while the rungs read its records. */
    json_t *body = NULL;
    if (cm_is_mapping(result)) {
        body = json_copy(result);
        json_obj_del(body, "kind");
    }
    /* Capture the projected messages into state FIRST (live drive assigns
     * state["messages"] = deep_mat copies before mat_result/stamp_live run, so
     * a raise leaves the mutation history at that point). No pop yet: the pop
     * happens inside stamp_live, after the result materializes. */
    json_t *msgs = json_array();
    if (messages && messages->type == JSON_ARRAY)
        for (size_t i = 0; i < messages->c.count; i++)
            json_append(msgs, cm_proj(json_get(messages, i)));
    json_set(ctx->state, "messages", msgs);
    /* Materialize/validate the result (live mat_result). A bad enum raises
     * here with the messages still pristine (no _canonical_row pop). */
    json_t *e = NULL;
    const json_t *outcomes = NULL, *uids = NULL, *rows = NULL;
    if (kind && strcmp(kind, "progress") == 0) {
        e = cm_progress_check(body);
        if (!e) { outcomes = cm_fget(body, "outcomes"); uids = cm_fget(body, "uids"); }
    } else if (kind && strcmp(kind, "batch") == 0) {
        e = cm_binding(body, "BatchAppendResult", BAR_FIELDS, 1, BAR_REQ, 1);
        if (!e) {
            outcomes = cm_fget(body, "outcomes");
            if (outcomes && outcomes->type == JSON_ARRAY)
                for (size_t i = 0; i < outcomes->c.count && !e; i++)
                    e = cm_row_outcome_check(json_get(outcomes, i));
        }
    } else if (kind && strcmp(kind, "replace") == 0) {
        e = cm_binding(body, "ReplaceResult", RR_FIELDS, 1, RR_REQ, 1);
        if (!e) {
            rows = cm_fget(body, "rows");
            if (rows && rows->type == JSON_ARRAY)
                for (size_t i = 0; i < rows->c.count && !e; i++)
                    e = cm_row_stamp_check(json_get(rows, i));
        }
    } else if (kind) {
        char msg[128];
        snprintf(msg, sizeof msg, "unknown result kind '%s'", kind);
        e = cm_err("RuntimeError", msg);
    }
    if (e) { json_free(body); return e; }
    /* stamp_live: pop _canonical_row from every message, then stamp. */
    for (size_t i = 0; i < msgs->c.count; i++) json_obj_del(json_get(msgs, i), "_canonical_row");
    json_t *ret = json_null();
    if (kind && strcmp(kind, "progress") == 0) {
        size_t n = json_len(msgs);
        size_t m = (uids && uids->type == JSON_ARRAY) ? uids->c.count : 0;
        for (size_t i = 0; i < n && i < m; i++) {
            json_t *msg = json_get(msgs, i);
            const json_t *u = json_get(uids, i);
            const json_t *mu = cm_fget(u, "message_uid");
            if (!cm_is_null(mu)) json_set(msg, "message_uid", cm_proj(mu));
            const json_t *tcuid = cm_fget(u, "tool_call_uid");
            if (!cm_is_null(tcuid)) json_set(msg, "_tool_call_uid", cm_proj(tcuid));
            const json_t *tcu = cm_fget(u, "tool_call_uids");
            if (!cm_is_null(tcu)) json_set(msg, "_tool_call_uids", cm_proj(tcu));
        }
        e = cm_stamp_batch(msgs, outcomes);
        if (e) ret = e;
    } else if (kind && strcmp(kind, "batch") == 0) {
        e = cm_stamp_batch(msgs, outcomes);
        if (e) ret = e;
    } else if (kind && strcmp(kind, "replace") == 0) {
        e = cm_stamp_replace(msgs, rows);
        if (e) ret = e;
    }
    json_free(body);
    return ret; /* result None: only the _canonical_row pop happened */
}

/* PoP: cm_compaction_result_row_ids @ persistence/conversation/messages.py:CompactionResult.row_ids */
static json_t *cm_compaction_result_row_ids(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *e = cm_binding(args, "CompactionResult", CR_FIELDS, 2, CR_REQ, 2);
    if (e) return e;
    json_t *out = json_array();
    const json_t *rows = cm_fget(args, "rows");
    if (rows && rows->type == JSON_ARRAY)
        for (size_t i = 0; i < rows->c.count; i++) {
            const json_t *r = json_get(rows, i);
            e = cm_row_stamp_check(r);
            if (e) { json_free(out); return e; }
            json_append(out, cm_fproj(r, "id"));
        }
    return out;
}

/* PoP: cm_compaction_result_row_snapshots @ persistence/conversation/messages.py:CompactionResult.row_snapshots */
static json_t *cm_compaction_result_row_snapshots(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *e = cm_binding(args, "CompactionResult", CR_FIELDS, 2, CR_REQ, 2);
    if (e) return e;
    json_t *out = json_array();
    const json_t *rows = cm_fget(args, "rows");
    if (rows && rows->type == JSON_ARRAY)
        for (size_t i = 0; i < rows->c.count; i++) {
            const json_t *r = json_get(rows, i);
            e = cm_row_stamp_check(r);
            if (e) { json_free(out); return e; }
            json_append(out, cm_fproj(r, "db_row_snapshot"));
        }
    return out;
}

/* PoP: cm_cooldown_snapshot_post_init @ persistence/conversation/messages.py:CooldownSnapshot.__post_init__ */
static json_t *cm_cooldown_snapshot_post_init(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *spec = cm_fget(args, "fields");
    json_t *e = cm_binding(spec, "CooldownSnapshot", CD_FIELDS, 3, CD_REQ, 1);
    if (e) return e;
    const json_t *present = cm_fget(spec, "present");
    const json_t *until = cm_fget(spec, "until");
    const json_t *error = cm_fget(spec, "error");
    if (!cm_truthy(present) && (!cm_is_null(until) || !cm_is_null(error)))
        return cm_err("ValueError", "an absent CooldownSnapshot cannot carry stored fields");
    return json_null();
}

/* PoP: cm_conversation_revision_as_tuple @ persistence/conversation/messages.py:ConversationRevision.as_tuple */
static json_t *cm_conversation_revision_as_tuple(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *spec = cm_fget(args, "fields");
    json_t *e = cm_binding(spec, "ConversationRevision", CV_FIELDS, 4, CV_REQ, 4);
    if (e) return e;
    json_t *out = json_array();
    json_append(out, cm_fproj(spec, "lineage_ids"));
    json_append(out, cm_fproj(spec, "active_count"));
    json_append(out, cm_fproj(spec, "max_active_id"));
    json_append(out, cm_fproj(spec, "transcript_revision"));
    return out;
}

/* PoP: cm_resume_too_large_init @ persistence/conversation/messages.py:ResumeTooLarge.__init__ */
static json_t *cm_resume_too_large_init(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    double mc = cm_num(cm_fget(args, "message_count"));
    double limit = cm_num(cm_fget(args, "limit"));
    const json_t *scope = cm_fget(args, "scope");
    const char *sc = cm_is_str(scope) ? scope->str_val : "across its lineage";
    char msg[1024];
    snprintf(msg, sizeof msg,
             "This session is too long to reload safely (%.0f messages; limit %.0f). "
             "Start a fresh chat and keep a copy with the dashboard Sessions page's Export action or "
             "`hermes sessions export --format md --session-id <id>` (neither is capped), or raise the limit "
             "with `hermes config set sessions.max_resume_messages 0`.",
             mc, limit);
    json_t *out = json_object();
    json_set_hdr(out, "class", "ResumeTooLarge");
    json_set_hdr(out, "message", msg);
    json_set(out, "message_count", cm_fproj(args, "message_count"));
    json_set(out, "limit", cm_fproj(args, "limit"));
    json_set_hdr(out, "scope", sc);
    json_set(out, "is_persistence_error", json_bool(true));
    json_set(out, "is_value_error", json_bool(true));
    return out;
}

/* DelegationDelivery validation shared by __post_init__ and identity. Returns
 * a raise envelope or NULL; *meta_out borrows the metadata node on success. */
static json_t *cm_delegation_check(const json_t *spec, const json_t **meta_out) {
    json_t *e = cm_binding(spec, "DelegationDelivery", DD_FIELDS, 2, DD_REQ, 2);
    if (e) return e;
    const json_t *meta = cm_fget(spec, "metadata");
    if (!cm_is_mapping(meta)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", cm_typename(meta));
        return cm_err("AttributeError", msg);
    }
    if (!cm_truthy(cm_fget(meta, "delegation_id")))
        return cm_err("ValueError", "Delegation delivery requires a stable delegation_id");
    *meta_out = meta;
    return NULL;
}

/* PoP: cm_delegation_delivery_post_init @ persistence/conversation/messages.py:DelegationDelivery.__post_init__ */
static json_t *cm_delegation_delivery_post_init(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *meta = NULL;
    json_t *e = cm_delegation_check(cm_fget(args, "fields"), &meta);
    return e ? e : json_null();
}

/* PoP: cm_delegation_delivery_identity @ persistence/conversation/messages.py:DelegationDelivery.identity */
static json_t *cm_delegation_delivery_identity(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *meta = NULL;
    json_t *e = cm_delegation_check(cm_fget(args, "fields"), &meta);
    if (e) return e;
    json_t *out = json_array();
    json_append(out, cm_proj(cm_fget(meta, "delegation_id")));
    const json_t *notice = cm_fget(meta, "delivery_notice");
    json_append(out, notice ? cm_proj(notice) : json_string(""));
    return out;
}

/* _CONVERSATION_KEYS emission order (key -> record field). */
static const char *const CONV_KEYS[][2] = {
    {"_db_row_snapshot", "db_row_snapshot"}, {"_row_id", "row_id"}, {"message_uid", "message_uid"},
    {"_tool_call_uids", "tool_call_uids"}, {"_absorbed_message_uids", "absorbed_message_uids"},
    {"api_content", "api_content"}, {"display_kind", "display_kind"}, {"display_metadata", "display_metadata"},
    {"data_use", "data_use"}, {"_compressed_summary", "compressed_summary"}, {"timestamp", "timestamp"},
    {"tool_call_id", "tool_call_id"}, {"tool_name", "tool_name"}, {"effect_disposition", "effect_disposition"},
    {"tool_calls", "tool_calls"}, {"message_id", "message_id"}, {"observed", "observed"},
    {"finish_reason", "finish_reason"}, {"reasoning", "reasoning"}, {"reasoning_content", "reasoning_content"},
    {"reasoning_details", "reasoning_details"}, {"codex_reasoning_items", "codex_reasoning_items"},
    {"codex_message_items", "codex_message_items"}, {"_tool_call_uid", "tool_call_uid"},
    {"_absorbed_row_ids", "absorbed_row_ids"}, {"_merged_turn_prefix", "merged_turn_prefix"},
    {"_merged_durable_rows", "merged_durable_rows"}, {"_unnamed_durable_rows", "unnamed_durable_rows"},
    {"_retired_durable_rows", "retired_durable_rows"}};
#define CONV_KEY_COUNT (sizeof(CONV_KEYS) / sizeof(CONV_KEYS[0]))

static bool cm_present_when_not_none(const char *key) {
    return strcmp(key, "reasoning_content") == 0 || strcmp(key, "reasoning_details") == 0 ||
           strcmp(key, "codex_reasoning_items") == 0 || strcmp(key, "codex_message_items") == 0;
}

/* PoP: cm_conversation_message_as_openai_message @ persistence/conversation/messages.py:ConversationMessage.as_openai_message */
static json_t *cm_conversation_message_as_openai_message(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *spec = cm_fget(args, "fields");
    json_t *e = cm_binding(spec, "ConversationMessage", CM_FIELDS, 33, CM_REQ, 2);
    if (e) return e;
    json_t *out = json_object();
    json_set(out, "role", cm_fproj(spec, "role"));
    json_set(out, "content", cm_fproj(spec, "content"));
    const json_t *dbp = cm_fget(spec, "db_persisted");
    bool persisted = dbp ? cm_truthy(dbp) : true; /* default True */
    if (persisted) json_set(out, "_db_persisted", json_bool(true));
    bool stored = cm_truthy(cm_fget(spec, "_tool_call_uid_stored")); /* default False */
    const json_t *tcuid = cm_fget(spec, "tool_call_uid");
    for (size_t i = 0; i < CONV_KEY_COUNT; i++) {
        const char *key = CONV_KEYS[i][0];
        const char *field = CONV_KEYS[i][1];
        if (strcmp(key, "_tool_call_uid") == 0 && stored) continue;
        const json_t *value = cm_fget(spec, field);
        if (strcmp(key, "_absorbed_message_uids") == 0 && stored && cm_truthy(tcuid))
            json_set(out, "_tool_call_uid", cm_proj(tcuid));
        if (cm_present_when_not_none(key)) {
            if (!cm_is_null(value)) json_set(out, key, cm_proj(value));
            continue;
        }
        if (!cm_truthy(value)) continue; /* every default is falsy */
        json_t *wire = NULL;
        if (strcmp(key, "_row_id") == 0) wire = cm_int_val(value);
        else if (strcmp(key, "tool_calls") == 0 || strcmp(key, "_absorbed_message_uids") == 0 ||
                 strcmp(key, "data_use") == 0) wire = cm_list_val(value);
        else if (strcmp(key, "_absorbed_row_ids") == 0) {
            wire = cm_list_val(value);
            if (!cm_is_raise(wire)) {
                json_t *ids = json_array();
                for (size_t k = 0; k < wire->c.count; k++) {
                    json_t *iv = cm_int_val(json_get(wire, k));
                    if (cm_is_raise(iv)) { json_free(ids); json_free(wire); wire = iv; break; }
                    json_append(ids, iv);
                }
                /* ids is already freed on the raise branch above; only the
                 * success path adopts it (freeing it again here is a
                 * double-free -> STATUS_HEAP_CORRUPTION). */
                if (!cm_is_raise(wire)) { json_free(wire); wire = ids; }
            }
        } else wire = cm_proj(value);
        if (cm_is_raise(wire)) { json_free(out); return wire; }
        json_set(out, key, wire);
    }
    return out;
}

/* _CONVERSATION_FIELD_BY_KEY + _FROM_WIRE conversion kind. */
typedef enum { CM_WIRE_NONE, CM_WIRE_INT, CM_WIRE_TUPLE, CM_WIRE_IDS, CM_WIRE_BOOL } cm_wire_kind;
static const struct { const char *key; const char *field; cm_wire_kind kind; } CONV_FROM[] = {
    {"_db_row_snapshot", "db_row_snapshot", CM_WIRE_NONE}, {"_row_id", "row_id", CM_WIRE_INT},
    {"message_uid", "message_uid", CM_WIRE_NONE}, {"_tool_call_uids", "tool_call_uids", CM_WIRE_NONE},
    {"_absorbed_message_uids", "absorbed_message_uids", CM_WIRE_TUPLE}, {"api_content", "api_content", CM_WIRE_NONE},
    {"display_kind", "display_kind", CM_WIRE_NONE}, {"display_metadata", "display_metadata", CM_WIRE_NONE},
    {"data_use", "data_use", CM_WIRE_TUPLE}, {"_compressed_summary", "compressed_summary", CM_WIRE_BOOL},
    {"timestamp", "timestamp", CM_WIRE_NONE}, {"tool_call_id", "tool_call_id", CM_WIRE_NONE},
    {"tool_name", "tool_name", CM_WIRE_NONE}, {"effect_disposition", "effect_disposition", CM_WIRE_NONE},
    {"tool_calls", "tool_calls", CM_WIRE_TUPLE}, {"message_id", "message_id", CM_WIRE_NONE},
    {"observed", "observed", CM_WIRE_BOOL}, {"finish_reason", "finish_reason", CM_WIRE_NONE},
    {"reasoning", "reasoning", CM_WIRE_NONE}, {"reasoning_content", "reasoning_content", CM_WIRE_NONE},
    {"reasoning_details", "reasoning_details", CM_WIRE_NONE}, {"codex_reasoning_items", "codex_reasoning_items", CM_WIRE_NONE},
    {"codex_message_items", "codex_message_items", CM_WIRE_NONE}, {"_tool_call_uid", "tool_call_uid", CM_WIRE_NONE},
    {"_absorbed_row_ids", "absorbed_row_ids", CM_WIRE_IDS}, {"_merged_turn_prefix", "merged_turn_prefix", CM_WIRE_NONE},
    {"_merged_durable_rows", "merged_durable_rows", CM_WIRE_NONE}, {"_unnamed_durable_rows", "unnamed_durable_rows", CM_WIRE_NONE},
    {"_retired_durable_rows", "retired_durable_rows", CM_WIRE_NONE}};
#define CONV_FROM_COUNT (sizeof(CONV_FROM) / sizeof(CONV_FROM[0]))

/* _AFTER_IDENTITY_KEYS: the keys between _absorbed_message_uids and
 * _tool_call_uid in _CONVERSATION_KEYS (inclusive of the former). */
static bool cm_after_identity(const char *key) {
    static const char *const set[] = {
        "_absorbed_message_uids", "api_content", "display_kind", "display_metadata", "data_use",
        "_compressed_summary", "timestamp", "tool_call_id", "tool_name", "effect_disposition",
        "tool_calls", "message_id", "observed", "finish_reason", "reasoning", "reasoning_content",
        "reasoning_details", "codex_reasoning_items", "codex_message_items"};
    return cm_in_list(key, set, sizeof(set) / sizeof(set[0]));
}

/* PoP: cm_conversation_message_from_openai_message @ persistence/conversation/messages.py:ConversationMessage.from_openai_message */
static json_t *cm_conversation_message_from_openai_message(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *msg = cm_fget(args, "message");
    const char *marker = "_db_persisted";
    bool has_marker = json_has(msg, marker);
    if (has_marker && !cm_is_bool_true(cm_fget(msg, marker)))
        return cm_err("ValueError", "a present born-durable marker must be True");
    /* Build the record with every field default, in definition order. */
    json_t *rec = json_object();
    json_set(rec, "role", json_string(""));
    json_set(rec, "content", json_null());
    json_set(rec, "db_row_snapshot", json_null());
    json_set(rec, "row_id", json_null());
    json_set(rec, "message_uid", json_null());
    json_set(rec, "tool_call_uids", json_null());
    json_set(rec, "tool_call_uid", json_null());
    json_set(rec, "absorbed_message_uids", json_array());
    json_set(rec, "api_content", json_null());
    json_set(rec, "display_kind", json_null());
    json_set(rec, "display_metadata", json_null());
    json_set(rec, "data_use", json_array());
    json_set(rec, "compressed_summary", json_bool(false));
    json_set(rec, "timestamp", json_null());
    json_set(rec, "tool_call_id", json_null());
    json_set(rec, "tool_name", json_null());
    json_set(rec, "effect_disposition", json_null());
    json_set(rec, "tool_calls", json_null());
    json_set(rec, "message_id", json_null());
    json_set(rec, "observed", json_bool(false));
    json_set(rec, "finish_reason", json_null());
    json_set(rec, "reasoning", json_null());
    json_set(rec, "reasoning_content", json_null());
    json_set(rec, "reasoning_details", json_null());
    json_set(rec, "codex_reasoning_items", json_null());
    json_set(rec, "codex_message_items", json_null());
    json_set(rec, "absorbed_row_ids", json_array());
    json_set(rec, "merged_turn_prefix", json_null());
    json_set(rec, "merged_durable_rows", json_null());
    json_set(rec, "unnamed_durable_rows", json_null());
    json_set(rec, "retired_durable_rows", json_null());
    json_set(rec, "db_persisted", json_bool(has_marker));
    json_set(rec, "_tool_call_uid_stored", json_bool(false));
    /* _tool_call_uid_stored from the key order. */
    if (json_has(msg, "_tool_call_uid")) {
        size_t uid_at = 0;
        for (size_t i = 0; i < msg->c.count; i++)
            if (strcmp(msg->c.keys[i], "_tool_call_uid") == 0) { uid_at = i; break; }
        bool any_after = false;
        for (size_t i = 0; i < uid_at; i++)
            if (cm_after_identity(msg->c.keys[i])) { any_after = true; break; }
        json_set(rec, "_tool_call_uid_stored", json_bool(!any_after));
    }
    /* Iterate the message keys in order, converting each known key. */
    if (msg && msg->type == JSON_OBJECT)
        for (size_t i = 0; i < msg->c.count; i++) {
            const char *key = msg->c.keys[i];
            if (strcmp(key, "role") == 0 || strcmp(key, "content") == 0 || strcmp(key, marker) == 0) continue;
            const char *field = NULL;
            cm_wire_kind kind = CM_WIRE_NONE;
            for (size_t k = 0; k < CONV_FROM_COUNT; k++)
                if (strcmp(CONV_FROM[k].key, key) == 0) { field = CONV_FROM[k].field; kind = CONV_FROM[k].kind; break; }
            if (!field) {
                char *r = cm_repr_str(key);
                char m[256];
                snprintf(m, sizeof m, "unknown replay projection key %s", r ? r : "''");
                free(r);
                json_free(rec);
                return cm_err("ValueError", m);
            }
            const json_t *value = msg->c.items[i];
            json_t *conv = NULL;
            if (kind == CM_WIRE_INT) conv = cm_int_val(value);
            else if (kind == CM_WIRE_BOOL) conv = cm_bool_val(value);
            else if (kind == CM_WIRE_TUPLE) { conv = cm_tuple_val(value); if (!cm_is_raise(conv)) { json_t *p = cm_proj(conv); json_free(conv); conv = p; } }
            else if (kind == CM_WIRE_IDS) {
                conv = cm_list_val(value);
                if (!cm_is_raise(conv)) {
                    json_t *ids = json_array();
                    for (size_t k = 0; k < conv->c.count; k++) {
                        json_t *iv = cm_int_val(json_get(conv, k));
                        if (cm_is_raise(iv)) { json_free(ids); json_free(conv); conv = iv; break; }
                        json_append(ids, iv);
                    }
                    /* ids is already freed on the raise branch above; only
                     * the success path adopts it (a second free here is a
                     * double-free -> STATUS_HEAP_CORRUPTION). */
                    if (!cm_is_raise(conv)) { json_free(conv); conv = ids; }
                }
            } else conv = cm_proj(value);
            if (cm_is_raise(conv)) { json_free(rec); return conv; }
            json_set(rec, field, conv);
        }
    /* role = str(message["role"]) (KeyError when absent); content = get("content"). */
    if (!json_has(msg, "role")) { json_free(rec); return cm_err("KeyError", "'role'"); }
    json_t *role = cm_str_val(cm_fget(msg, "role"));
    json_set(rec, "role", role);
    json_set(rec, "content", cm_fproj(msg, "content"));
    return rec;
}

/* PoP: cm_transcript_read_post_init @ persistence/conversation/messages.py:TranscriptRead.__post_init__ */
static json_t *cm_transcript_read_post_init(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *spec = cm_fget(args, "fields");
    json_t *e = cm_binding(spec, "TranscriptRead", TR_FIELDS, 6, NULL, 0);
    if (e) return e;
    const json_t *view = cm_fget(spec, "view");
    const char *view_s = "active";
    if (view) {
        e = cm_enum(view, TRANSCRIPT_VIEW, 3, "TranscriptView");
        if (e) return e;
        view_s = cm_str(view);
    }
    const json_t *after = cm_fget(spec, "after_id");
    if (!cm_is_null(after)) {
        if (cm_truthy(cm_fget(spec, "latest")) || cm_truthy(cm_fget(spec, "offset")))
            return cm_err("ValueError", "after_id is incompatible with latest/offset paging");
        if (strcmp(view_s, "display") == 0)
            return cm_err("ValueError", "after_id is incompatible with include_compacted (deduped display reads use offset paging)");
        if (cm_truthy(cm_fget(spec, "include_lineage")))
            return cm_err("ValueError", "after_id is incompatible with include_ancestors (merged-lineage reads use offset paging)");
    }
    return json_null();
}

/* _WIRE_ROW_KEYS emission order. */
static const char *const WIRE_ROW_KEYS[] = {
    "id", "session_id", "role", "content", "tool_call_id", "tool_calls", "tool_name", "effect_disposition",
    "timestamp", "token_count", "finish_reason", "reasoning", "reasoning_content", "reasoning_details",
    "codex_reasoning_items", "codex_message_items", "platform_message_id", "observed", "active", "compacted",
    "api_content", "display_kind", "display_metadata", "message_uid", "absorbed_message_uids", "tool_call_uids",
    "tool_call_uid"};
#define WIRE_ROW_KEY_COUNT (sizeof(WIRE_ROW_KEYS) / sizeof(WIRE_ROW_KEYS[0]))

/* PoP: cm_transcript_row_as_wire_row @ persistence/conversation/messages.py:TranscriptRow.as_wire_row */
static json_t *cm_transcript_row_as_wire_row(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *spec = cm_fget(args, "fields");
    json_t *e = cm_binding(spec, "TranscriptRow", TW_FIELDS, 27, TW_REQ, 6);
    if (e) return e;
    const json_t *vis = cm_fget(spec, "visibility");
    e = cm_enum(vis, ROW_VISIBILITY, 3, "RowVisibility");
    if (e) return e;
    const char *vs = cm_str(vis);
    double active = 0, compacted = 0;
    if (strcmp(vs, "active") == 0) { active = 1; compacted = 0; }
    else if (strcmp(vs, "compaction_archived") == 0) { active = 0; compacted = 1; }
    else { active = 0; compacted = 0; } /* rewound */
    json_t *id = cm_int_val(cm_fget(spec, "id"));
    if (cm_is_raise(id)) return id;
    json_t *session = cm_str_val(cm_fget(spec, "session_id"));
    double observed = cm_truthy(cm_fget(spec, "observed")) ? 1 : 0;
    json_t *out = json_object();
    for (size_t i = 0; i < WIRE_ROW_KEY_COUNT; i++) {
        const char *key = WIRE_ROW_KEYS[i];
        if (strcmp(key, "id") == 0) json_set(out, key, json_copy(id));
        else if (strcmp(key, "session_id") == 0) json_set(out, key, json_copy(session));
        else if (strcmp(key, "observed") == 0) json_set(out, key, json_number(observed));
        else if (strcmp(key, "active") == 0) json_set(out, key, json_number(active));
        else if (strcmp(key, "compacted") == 0) json_set(out, key, json_number(compacted));
        else json_set(out, key, cm_fproj(spec, key));
    }
    json_free(id);
    json_free(session);
    if (cm_truthy(cm_fget(spec, "compressed_summary"))) json_set(out, "_compressed_summary", json_bool(true));
    return out;
}

/* ReadCheck binding: check positional + pinned keyword-only. */
static json_t *cm_read_check_binding(const json_t *spec) {
    if (!json_has(spec, "check"))
        return cm_err("TypeError", "ReadCheck.__init__() missing 1 required positional argument: 'check'");
    if (!json_has(spec, "pinned"))
        return cm_err("TypeError", "ReadCheck.__init__() missing 1 required keyword-only argument: 'pinned'");
    return NULL;
}

/* PoP: cm_read_check_init @ persistence/conversation/messages.py:ReadCheck.__init__ */
static json_t *cm_read_check_init(cm_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    json_t *e = cm_read_check_binding(args);
    if (e) return e;
    json_t *out = json_object();
    json_set(out, "pinned", cm_fproj(args, "pinned"));
    return out;
}

/* PoP: cm_read_check_call @ persistence/conversation/messages.py:ReadCheck.__call__ */
static json_t *cm_read_check_call(cm_ctx_t *ctx, const json_t *args) {
    json_t *e = cm_read_check_binding(args);
    if (e) return e;
    const json_t *check = cm_fget(args, "check");
    if (cm_str(check) && strcmp(check->str_val, "seam") == 0) {
        json_t *ret = cm_call(ctx, "check", json_object());
        if (cm_is_raise(ret)) return ret;
        json_t *b = cm_bool_val(ret);
        json_free(ret);
        return b;
    }
    char msg[128];
    snprintf(msg, sizeof msg, "'%s' object is not callable", cm_typename(check));
    return cm_err("TypeError", msg);
}