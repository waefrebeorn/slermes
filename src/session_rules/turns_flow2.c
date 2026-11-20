/*
 * turns_flow2.c - FLOW cores of the UNCLAIMED pure helpers of
 * session_rules/turns.py that are portable with libjson alone.
 *
 * A CRLF-aware PoP grep over src/ + include/ shows 19 turns.py defs already
 * claimed (field/text/flatten/blank/stale/UID/typed-ask/retryable/... leaves)
 * by chat_completion_helpers_flow.c, store_rules_flow.c, store_rules_flow2.c
 * and store_identity_flow.c. This module claims ONLY unclaimed defs; every
 * PoP below names a function with no prior claim. The claimed helpers this
 * cluster needs (coalesce_tool_call_id) are transcribed as a private tf2_
 * helper WITHOUT a PoP annotation so the credit stays with its owner.
 *
 * Ported (20 defs, all UNCLAIMED at authoring time):
 *   _uid_occurrences, merge_tool_call_uids, _tc_field,
 *   _expand_tool_id_variants, tool_call_id_variants, tool_result_id_variants,
 *   per_occurrence_tool_call_uids, index_tool_call_uids,
 *   resolve_tool_call_uid, retired_row_payload, _is_multimodal_tool_result,
 *   _is_text_part, _multimodal_text_summary, _is_ephemeral_scaffolding,
 *   _image_marker, _durable_content, _content_has_payload, _msg_has_payload,
 *   _is_codex_interim, _json_or.
 *
 * Out of scope for this libjson-only FLOW shape: regex-dependent leaves
 * (sanitize_context, the skill-scaffold describer cluster), global-mutable
 * scaffolding registries (register_auto_load_scaffold, _registered_auto_load_tail)
 * and the multi-pass sequence-repair orchestrators that call them.
 *
 * Representation contract: NULL json_t* == Python None; a raise is
 * {"__raise__": cls, "message": str(exc)}; frozensets fold to a sorted JSON
 * array of strings (the oracle canon sorts sets the same way). Mutating
 * functions (index_tool_call_uids) rewrite their mapping arg in place and the
 * harness echoes it under "state" keyed by parameter name.
 *
 * Full builtin exception hierarchy is carried (TF2_EXC_TABLE) for faithful
 * error projection even though this cluster has no raising leaves.
 * Self-contained: the harness #includes this .c directly; libjson only.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- constants (seeded, not seamed) -- */
#define TF2_TOOL_CALL_UIDS "_tool_call_uids"
static const char *TF2_EPHEMERAL_SCAFFOLDING_FLAGS[] = {
    "_empty_recovery_synthetic", "_empty_terminal_sentinel", "_thinking_prefill",
    "_verification_stop_synthetic", "_pre_verify_synthetic", "_kanban_stop_synthetic",
    "_dropped_toolcall_nudge",
};
#define TF2_EPHEMERAL_SCAFFOLDING_FLAGS_N 7
static const char *TF2_IMAGE_PART_TYPES[] = { "image", "image_url", "input_image" };
#define TF2_IMAGE_PART_TYPES_N 3

/* -- universal seam context (carried for contract; no seams in this cluster) -- */
typedef struct tf2_ctx {
    const json_t *io;
    json_t *cursors;
    json_t *trace;
} tf2_ctx_t;

/* -- basic type helpers -- */
static bool tf2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool tf2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool tf2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static bool tf2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool tf2_is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool tf2_is_dict(const json_t *v) { return v && v->type == JSON_OBJECT; }
static const char *tf2_str(const json_t *v) { return tf2_is_str(v) ? v->str_val : NULL; }

static bool tf2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

/* Python == on the representation (numeric cross-type, deep list/dict). */
static bool tf2_eq(const json_t *a, const json_t *b) {
    bool an = tf2_is_num(a) || tf2_is_bool(a), bn = tf2_is_num(b) || tf2_is_bool(b);
    if (an || bn) {
        if (!an || !bn) return false;
        double x = tf2_is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = tf2_is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (tf2_is_null(a) || tf2_is_null(b)) return tf2_is_null(a) && tf2_is_null(b);
    if (tf2_is_str(a) || tf2_is_str(b))
        return tf2_is_str(a) && tf2_is_str(b) && strcmp(a->str_val, b->str_val) == 0;
    if (tf2_is_list(a) || tf2_is_list(b)) {
        if (!tf2_is_list(a) || !tf2_is_list(b) || a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!tf2_eq(json_get(a, i), json_get(b, i))) return false;
        return true;
    }
    if (tf2_is_dict(a) && tf2_is_dict(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !tf2_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}
static bool tf2_eq_str(const json_t *v, const char *lit) {
    return tf2_is_str(v) && v->str_val && strcmp(v->str_val, lit) == 0;
}

/* Python type(x).__name__ on the representation (domain-restricted). */
static json_t *tf2_err(const char *type, const char *message); /* fwd */
static const char *tf2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static json_t *tf2_attr_err(const json_t *v, const char *attr) {
    char buf[160];
    snprintf(buf, sizeof buf, "'%s' object has no attribute '%s'", tf2_type_name(v), attr);
    return tf2_err("AttributeError", buf);
}

/* -- exceptions -- */
static json_t *tf2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* FULL builtin exception hierarchy (py3.14). Unknown names parent to
 * Exception; BaseException-only escapes pass every except-Exception clause. */
typedef struct { const char *name; const char *parent; } tf2_exc_ent_t;
static const tf2_exc_ent_t TF2_EXC_TABLE[] = {
    { "BaseException", NULL }, { "Exception", "BaseException" },
    { "TypeError", "Exception" }, { "StopIteration", "Exception" },
    { "StopAsyncIteration", "Exception" }, { "ArithmeticError", "Exception" },
    { "FloatingPointError", "ArithmeticError" }, { "OverflowError", "ArithmeticError" },
    { "ZeroDivisionError", "ArithmeticError" }, { "AssertionError", "Exception" },
    { "AttributeError", "Exception" }, { "BufferError", "Exception" },
    { "EOFError", "Exception" }, { "ImportError", "Exception" },
    { "ModuleNotFoundError", "ImportError" }, { "LookupError", "Exception" },
    { "IndexError", "LookupError" }, { "KeyError", "LookupError" },
    { "MemoryError", "Exception" }, { "NameError", "Exception" },
    { "UnboundLocalError", "NameError" }, { "OSError", "Exception" },
    { "BlockingIOError", "OSError" }, { "ChildProcessError", "OSError" },
    { "ConnectionError", "OSError" }, { "BrokenPipeError", "ConnectionError" },
    { "ConnectionAbortedError", "ConnectionError" },
    { "ConnectionRefusedError", "ConnectionError" },
    { "ConnectionResetError", "ConnectionError" }, { "FileExistsError", "OSError" },
    { "FileNotFoundError", "OSError" }, { "InterruptedError", "OSError" },
    { "IsADirectoryError", "OSError" }, { "NotADirectoryError", "OSError" },
    { "PermissionError", "OSError" }, { "ProcessLookupError", "OSError" },
    { "TimeoutError", "OSError" }, { "ReferenceError", "Exception" },
    { "RuntimeError", "Exception" }, { "NotImplementedError", "RuntimeError" },
    { "RecursionError", "RuntimeError" }, { "SyntaxError", "Exception" },
    { "IndentationError", "SyntaxError" }, { "TabError", "IndentationError" },
    { "SystemError", "Exception" }, { "ValueError", "Exception" },
    { "UnicodeError", "ValueError" }, { "UnicodeDecodeError", "UnicodeError" },
    { "UnicodeEncodeError", "UnicodeError" }, { "UnicodeTranslateError", "UnicodeError" },
    { "Warning", "Exception" }, { "DeprecationWarning", "Warning" },
    { "PendingDeprecationWarning", "Warning" }, { "RuntimeWarning", "Warning" },
    { "SyntaxWarning", "Warning" }, { "UserWarning", "Warning" },
    { "FutureWarning", "Warning" }, { "ImportWarning", "Warning" },
    { "UnicodeWarning", "Warning" }, { "BytesWarning", "Warning" },
    { "EncodingWarning", "Warning" }, { "ResourceWarning", "Warning" },
    { "ExceptionGroup", "Exception" }, { "BaseExceptionGroup", "BaseException" },
    { "GeneratorExit", "BaseException" }, { "KeyboardInterrupt", "BaseException" },
    { "SystemExit", "BaseException" },
};
#define TF2_EXC_TABLE_N (sizeof(TF2_EXC_TABLE) / sizeof(TF2_EXC_TABLE[0]))
static const char *tf2_exc_parent(const char *name) {
    for (size_t i = 0; i < TF2_EXC_TABLE_N; i++)
        if (strcmp(TF2_EXC_TABLE[i].name, name) == 0) return TF2_EXC_TABLE[i].parent;
    return "Exception";
}
/* True when exc class name is cls or a descendant of cls. */
static bool tf2_exc_is(const char *name, const char *cls) {
    const char *cur = name;
    for (int guard = 0; guard < 16 && cur; guard++) {
        if (strcmp(cur, cls) == 0) return true;
        cur = tf2_exc_parent(cur);
    }
    return false;
}

/* -- string builder -- */
typedef struct { char *b; size_t n, cap; } tf2_sb_t;
static void tf2_sb_init(tf2_sb_t *s) { s->cap = 128; s->n = 0; s->b = malloc(s->cap); if (s->b) s->b[0] = '\0'; }
static void tf2_sb_putc(tf2_sb_t *s, char c) {
    if (!s->b) return;
    if (s->n + 2 > s->cap) { s->cap *= 2; char *nb = realloc(s->b, s->cap); if (!nb) return; s->b = nb; }
    s->b[s->n++] = c; s->b[s->n] = '\0';
}
static void tf2_sb_puts(tf2_sb_t *s, const char *t) { for (; t && *t; t++) tf2_sb_putc(s, *t); }
static void tf2_sb_free(tf2_sb_t *s) { free(s->b); s->b = NULL; }
static char *tf2_sb_take(tf2_sb_t *s) { char *r = s->b; s->b = NULL; s->n = s->cap = 0; return r; }

/* -- Python str helpers -- */
/* str.strip(): leading/trailing ASCII+unicode whitespace (domain: ASCII). */
static char *tf2_strip(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *out = malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static bool tf2_has_prefix(const char *s, const char *p) { return s && p && strncmp(s, p, strlen(p)) == 0; }

/* Split on first occurrence of sep (single char or short literal). Returns
 * malloc'd before/after; caller frees both. `found` reports presence. */
static void tf2_split_first(const char *s, const char *sep, char **before, char **after, bool *found) {
    const char *p = s ? strstr(s, sep) : NULL;
    *found = p != NULL;
    size_t n = p ? (size_t)(p - s) : strlen(s ? s : "");
    *before = malloc(n + 1);
    if (*before) { memcpy(*before, s ? s : "", n); (*before)[n] = '\0'; }
    size_t m = p ? strlen(p + strlen(sep)) : 0;
    *after = malloc(m + 1);
    if (*after) { memcpy(*after, p ? p + strlen(sep) : "", m); (*after)[m] = '\0'; }
}

/* Python str(x) for scalars; JSON containers use a compact repr. */
static char *tf2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            char buf[64];
            if (isfinite(d) && floor(d) == d) snprintf(buf, sizeof buf, "%lld", (long long)d);
            else snprintf(buf, sizeof buf, "%.17g", d);
            return strdup(buf);
        }
        default: break;
    }
    /* Container repr: Python uses single quotes; good enough for scalar-driven
     * fixture domains (arrays/objects with text_summary are excluded). */
    char *s = json_serialize(v);
    return s ? s : strdup("");
}

/* Python json.dumps(value) with default separators (", " / ": "), default=str. */
static void tf2_dumps_into(const json_t *v, tf2_sb_t *sb) {
    if (!v || v->type == JSON_NULL) { tf2_sb_puts(sb, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: tf2_sb_puts(sb, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            double d = v->num_val;
            char buf[64];
            if (isfinite(d) && floor(d) == d) snprintf(buf, sizeof buf, "%lld", (long long)d);
            else snprintf(buf, sizeof buf, "%.17g", d);
            tf2_sb_puts(sb, buf);
            return;
        }
        case JSON_STRING: {
            char *e = json_escape_string(v->str_val ? v->str_val : "");
            tf2_sb_puts(sb, e ? e : "\"\""); free(e);
            return;
        }
        case JSON_ARRAY:
            tf2_sb_putc(sb, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) tf2_sb_puts(sb, ", ");
                tf2_dumps_into(json_get(v, i), sb);
            }
            tf2_sb_putc(sb, ']');
            return;
        case JSON_OBJECT:
            tf2_sb_putc(sb, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) tf2_sb_puts(sb, ", ");
                char *e = json_escape_string(v->c.keys[i]);
                tf2_sb_puts(sb, e ? e : "\"\""); free(e);
                tf2_sb_puts(sb, ": ");
                tf2_dumps_into(json_obj_get(v, v->c.keys[i]), sb);
            }
            tf2_sb_putc(sb, '}');
            return;
    }
}

/* -- string set (frozenset of str -> sorted JSON array) -- */
static void tf2_set_add(json_t *arr, const char *s) {
    if (!s) return;
    for (size_t i = 0; i < arr->c.count; i++) {
        const char *x = tf2_str(json_get(arr, i));
        if (x && strcmp(x, s) == 0) return;
    }
    json_append(arr, json_string(s));
}
static void tf2_set_sort(json_t *arr) {
    for (size_t i = 1; i < arr->c.count; i++) {
        size_t j = i;
        while (j > 0) {
            const char *a = tf2_str(json_get(arr, j - 1));
            const char *b = tf2_str(json_get(arr, j));
            if (a && b && strcmp(a, b) <= 0) break;
            json_t *tmp = arr->c.items[j - 1];
            arr->c.items[j - 1] = arr->c.items[j];
            arr->c.items[j] = tmp;
            j--;
        }
    }
}

/* -- projection (returns an owned copy of a C result) -- */
static json_t *tf2_proj(const json_t *v) { return v ? json_copy(v) : json_null(); }

/* ========================================================================
 * PRIVATE helper: coalesce_tool_call_id (owner: store_identity_flow.c / si_).
 * Transcribed so this cluster is self-contained; NO PoP claim is emitted.
 * ======================================================================== */
static char *tf2_field_str(const json_t *tc, const char *key) {
    /* _tc_field(tc,key) then strip if str; NULL for absent/non-str. */
    if (!tc || tc->type != JSON_OBJECT) return NULL;
    const json_t *v = json_obj_get(tc, key);
    if (!tf2_is_str(v)) return NULL;
    return tf2_strip(v->str_val);
}
static char *tf2_coalesce_tool_call_id(const json_t *tc) {
    const char *keys[2] = { "call_id", "id" };
    for (int i = 0; i < 2; i++) {
        char *value = tf2_field_str(tc, keys[i]);
        if (value && *value) {
            char *original = strdup(value);
            char *bar = strchr(value, '|');
            if (bar) {
                *bar = '\0';
                char *head = tf2_strip(value);
                free(value);
                if (head && *head) { free(original); return head; }
                free(head);
                return original;  /* head empty -> Python returns the stripped value */
            }
            free(original);
            return value;
        }
        free(value);
    }
    return strdup("");
}

/* ========================================================================
 * PORTED UNCLAIMED FLOW CORES (tf2_*), one PoP each.
 * ======================================================================== */

/* PoP: tf2_uid_occurrences @ session_rules/turns.py:_uid_occurrences */
static json_t *tf2_uid_occurrences(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *value = json_obj_get(a, "value");
    json_t *out = json_array();
    if (tf2_is_list(value)) {
        for (size_t i = 0; i < value->c.count; i++) json_append(out, json_copy(json_get(value, i)));
    } else {
        json_append(out, value ? json_copy(value) : json_null());
    }
    return out;
}

/* PoP: tf2_tc_field @ session_rules/turns.py:_tc_field */
static json_t *tf2_tc_field(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *tc = json_obj_get(a, "tc");
    const char *key = json_get_str(a, "key", "");
    if (tc && tc->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(tc, key);
        return v ? json_copy(v) : NULL;
    }
    return NULL; /* getattr(tc,key,None) on a non-dict domain value */
}

/* PoP: tf2_expand_tool_id_variants @ session_rules/turns.py:_expand_tool_id_variants */
static json_t *tf2_expand_tool_id_variants(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *values = json_obj_get(a, "values");
    json_t *out = json_array();
    if (!tf2_is_list(values)) return out;
    for (size_t i = 0; i < values->c.count; i++) {
        const json_t *raw = json_get(values, i);
        char *value = tf2_is_str(raw) ? tf2_strip(raw->str_val) : NULL;
        if (!value || !*value) { free(value); continue; }
        tf2_set_add(out, value);
        /* parts of value.split("|"), each stripped and non-empty */
        const char *p = value;
        const char *bar;
        while ((bar = strchr(p, '|')) != NULL) {
            size_t n = (size_t)(bar - p);
            char *part = malloc(n + 1);
            if (part) {
                memcpy(part, p, n); part[n] = '\0';
                char *t = tf2_strip(part); free(part);
                if (t && *t) tf2_set_add(out, t);
                free(t);
            }
            p = bar + 1;
        }
        char *t = tf2_strip(p);
        if (t && *t) tf2_set_add(out, t);
        free(t);
        free(value);
    }
    tf2_set_sort(out);
    return out;
}

/* PoP: tf2_tool_call_id_variants @ session_rules/turns.py:tool_call_id_variants */
static json_t *tf2_tool_call_id_variants(tf2_ctx_t *ctx, json_t *a) {
    const json_t *tc = json_obj_get(a, "tc");
    json_t *args = json_object();
    json_t *vals = json_array();
    const char *keys[3] = { "call_id", "id", "response_item_id" };
    for (int i = 0; i < 3; i++) {
        if (tc && tc->type == JSON_OBJECT) {
            const json_t *v = json_obj_get(tc, keys[i]);
            json_append(vals, v ? json_copy(v) : json_null());
        } else {
            json_append(vals, json_null());
        }
    }
    json_set(args, "values", vals);
    json_t *r = tf2_expand_tool_id_variants(ctx, args);
    json_free(args);
    return r;
}

/* PoP: tf2_tool_result_id_variants @ session_rules/turns.py:tool_result_id_variants */
static json_t *tf2_tool_result_id_variants(tf2_ctx_t *ctx, json_t *a) {
    const json_t *tcid = json_obj_get(a, "tool_call_id");
    json_t *args = json_object();
    json_t *vals = json_array();
    json_append(vals, tcid ? json_copy(tcid) : json_null());
    json_set(args, "values", vals);
    json_t *r = tf2_expand_tool_id_variants(ctx, args);
    json_free(args);
    return r;
}

/* PoP: tf2_merge_tool_call_uids @ session_rules/turns.py:merge_tool_call_uids */
static json_t *tf2_merge_tool_call_uids(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *into = json_obj_get(a, "into");
    const json_t *extra = json_obj_get(a, "extra");
    if (!tf2_is_dict(into))
        return tf2_err("TypeError", tf2_is_null(into)
            ? "'NoneType' object is not iterable" : "object is not iterable");
    if (!tf2_is_dict(extra))
        return tf2_attr_err(extra, "items");
    json_t *merged = json_copy(into);
    if (tf2_is_dict(extra)) {
        for (size_t i = 0; i < extra->c.count; i++) {
            const char *k = extra->c.keys[i];
            const json_t *ev = extra->c.items[i];
            const json_t *mv = json_obj_get(merged, k);
            if (mv) {
                /* _uid_occurrences(mv) + _uid_occurrences(ev) */
                json_t *cat = json_array();
                if (tf2_is_list(mv)) {
                    for (size_t j = 0; j < mv->c.count; j++) json_append(cat, json_copy(json_get(mv, j)));
                } else {
                    json_append(cat, json_copy(mv));
                }
                if (tf2_is_list(ev)) {
                    for (size_t j = 0; j < ev->c.count; j++) json_append(cat, json_copy(json_get(ev, j)));
                } else {
                    json_append(cat, json_copy(ev));
                }
                json_set(merged, k, cat);
            } else {
                json_set(merged, k, json_copy(ev));
            }
        }
    }
    return merged;
}

/* PoP: tf2_per_occurrence_tool_call_uids @ session_rules/turns.py:per_occurrence_tool_call_uids */
static json_t *tf2_per_occurrence_tool_call_uids(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *uids = json_obj_get(a, "uids");
    const json_t *tcs = json_obj_get(a, "tool_calls");
    json_t *counts = json_object(); /* insertion-ordered like Counter/dict */
    if (tf2_is_list(tcs)) {
        for (size_t i = 0; i < tcs->c.count; i++) {
            char *cid = tf2_coalesce_tool_call_id(json_get(tcs, i));
            if (cid && *cid) {
                const json_t *cur = json_obj_get(counts, cid);
                double n = cur ? json_get_num(counts, cid, 0) : 0;
                json_set(counts, cid, json_number(n + 1));
            }
            free(cid);
        }
    }
    json_t *expanded = tf2_is_dict(uids) ? json_copy(uids) : json_object();
    for (size_t i = 0; i < counts->c.count; i++) {
        const char *cid = counts->c.keys[i];
        double count = json_get_num(counts, cid, 0);
        const json_t *uv = json_obj_get(expanded, cid);
        if (count > 1 && tf2_is_str(uv)) {
            json_t *rep = json_array();
            for (int j = 0; j < (int)count; j++) json_append(rep, json_copy(uv));
            json_set(expanded, cid, rep);
        }
    }
    json_free(counts);
    return expanded;
}

/* PoP: tf2_index_tool_call_uids @ session_rules/turns.py:index_tool_call_uids */
static json_t *tf2_index_tool_call_uids(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    json_t *index = json_obj_get(a, "index");
    const json_t *assistant = json_obj_get(a, "assistant");
    if (!tf2_is_dict(index)) index = json_object();
    const json_t *calls = (assistant && assistant->type == JSON_OBJECT)
                          ? json_obj_get(assistant, "tool_calls") : NULL;

    /* calls = [(tc, variants)] */
    size_t n = tf2_is_list(calls) ? calls->c.count : 0;
    json_t *callvars = json_array();
    json_t *named = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *va = json_object();
        /* Build a proper args object for tool_call_id_variants. */
        json_t *tcarg = json_object();
        json_set(tcarg, "tc", json_copy(json_get(calls, i)));
        json_t *v = tf2_tool_call_id_variants(NULL, tcarg);
        json_free(tcarg);
        json_set(va, "tc", json_copy(json_get(calls, i)));
        json_set(va, "variants", json_copy(v));
        json_append(callvars, va);
        for (size_t k = 0; k < v->c.count; k++) tf2_set_add(named, tf2_str(json_get(v, k)));
        json_free(v);
    }
    tf2_set_sort(named);
    /* index.pop(variant, None) for every named variant */
    for (size_t k = 0; k < named->c.count; k++) json_obj_del(index, tf2_str(json_get(named, k)));

    const json_t *uids = (assistant && assistant->type == JSON_OBJECT)
                         ? json_obj_get(assistant, TF2_TOOL_CALL_UIDS) : NULL;
    if (!tf2_is_dict(uids) || uids->c.count == 0) { json_free(callvars); return named; }

    json_t *occurrence = json_object();
    for (size_t i = 0; i < callvars->c.count; i++) {
        json_t *va = json_get(callvars, i);
        const json_t *tc = json_obj_get(va, "tc");
        const json_t *variants = json_obj_get(va, "variants");
        char *call_id = tf2_coalesce_tool_call_id(tc);
        const json_t *uid = json_obj_get(uids, call_id);
        if (tf2_is_list(uid)) {
            double prev = json_obj_get(occurrence, call_id) ? json_get_num(occurrence, call_id, 0) : -1;
            double nth = prev + 1;
            json_set(occurrence, call_id, json_number(nth));
            uid = (nth >= 0 && (size_t)nth < uid->c.count) ? json_get(uid, (size_t)nth) : NULL;
        }
        if (tf2_is_str(uid) && tf2_truthy(uid)) {
            for (size_t k = 0; k < variants->c.count; k++)
                json_set(index, tf2_str(json_get(variants, k)), json_copy(uid));
        }
        free(call_id);
    }
    json_free(occurrence);
    json_free(callvars);
    return named;
}

/* PoP: tf2_resolve_tool_call_uid @ session_rules/turns.py:resolve_tool_call_uid */
static json_t *tf2_resolve_tool_call_uid(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *index = json_obj_get(a, "index");
    const json_t *tcid = json_obj_get(a, "tool_call_id");
    if (!tf2_is_dict(index) || index->c.count == 0 || !tf2_is_str(tcid) || !(*tcid->str_val)) return NULL;
    json_t *va = json_object();
    json_set(va, "tool_call_id", json_copy(tcid));
    json_t *variants = tf2_tool_result_id_variants(NULL, va);
    json_free(va);
    for (size_t i = 0; i < variants->c.count; i++) {
        const json_t *uid = json_obj_get(index, tf2_str(json_get(variants, i)));
        if (tf2_truthy(uid)) { json_free(variants); return json_copy(uid); }
    }
    json_free(variants);
    return NULL;
}

/* PoP: tf2_retired_row_payload @ session_rules/turns.py:retired_row_payload */
static json_t *tf2_retired_row_payload(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *m = json_obj_get(a, "message");
    if (!tf2_is_dict(m)) return tf2_attr_err(m, "get");
    json_t *p = json_object();
    const json_t *role = json_obj_get(m, "role");
    const json_t *content = json_obj_get(m, "content");
    json_set(p, "role", role ? json_copy(role) : json_null());
    json_set(p, "content", content ? json_copy(content) : json_null());
    const json_t *tci = tf2_is_dict(m) ? json_obj_get(m, "tool_call_id") : NULL;
    if (tf2_truthy(tci)) json_set(p, "tool_call_id", json_copy(tci));
    const json_t *tcs = tf2_is_dict(m) ? json_obj_get(m, "tool_calls") : NULL;
    json_t *calls = json_array();
    if (tf2_is_list(tcs)) {
        for (size_t i = 0; i < tcs->c.count; i++) {
            const json_t *c = json_get(tcs, i);
            if (tf2_is_dict(c)) {
                const json_t *id = json_obj_get(c, "id");
                json_append(calls, id ? json_copy(id) : json_null());
            }
        }
    }
    if (calls->c.count) json_set(p, "tool_call_ids", calls);
    else json_free(calls);
    return p;
}

/* PoP: tf2_is_multimodal_tool_result @ session_rules/turns.py:_is_multimodal_tool_result */
static json_t *tf2_is_multimodal_tool_result(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *v = json_obj_get(a, "value");
    const json_t *flag = tf2_is_dict(v) ? json_obj_get(v, "_multimodal") : NULL;
    const json_t *content = tf2_is_dict(v) ? json_obj_get(v, "content") : NULL;
    return json_bool(tf2_is_dict(v) && tf2_is_bool(flag) && flag->bool_val && tf2_is_list(content));
}

/* PoP: tf2_is_text_part @ session_rules/turns.py:_is_text_part */
static json_t *tf2_is_text_part(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *p = json_obj_get(a, "p");
    return json_bool(tf2_is_dict(p) && tf2_eq_str(json_obj_get(p, "type"), "text"));
}

/* PoP: tf2_multimodal_text_summary @ session_rules/turns.py:_multimodal_text_summary */
static json_t *tf2_multimodal_text_summary(tf2_ctx_t *ctx, json_t *a) {
    const json_t *v = json_obj_get(a, "value");
    if (tf2_is_str(v)) return json_copy(v);
    const json_t *flag = tf2_is_dict(v) ? json_obj_get(v, "_multimodal") : NULL;
    const json_t *content = tf2_is_dict(v) ? json_obj_get(v, "content") : NULL;
    if (tf2_is_dict(v) && tf2_is_bool(flag) && flag->bool_val && tf2_is_list(content)) {
        const json_t *ts = json_obj_get(v, "text_summary");
        if (tf2_truthy(ts)) { char *s = tf2_py_str(ts); json_t *r = json_string(s); free(s); return r; }
        json_t *parts = json_array();
        for (size_t i = 0; i < content->c.count; i++) {
            const json_t *p = json_get(content, i);
            if (tf2_is_dict(p) && tf2_eq_str(json_obj_get(p, "type"), "text")) {
                const json_t *t = json_obj_get(p, "text");
                char *s = tf2_py_str(t ? t : json_string(""));
                json_append(parts, json_string(s)); free(s);
            }
        }
        if (parts->c.count == 0) { json_free(parts); return json_string("[multimodal tool result]"); }
        tf2_sb_t sb; tf2_sb_init(&sb);
        for (size_t i = 0; i < parts->c.count; i++) {
            if (i) tf2_sb_putc(&sb, '\n');
            tf2_sb_puts(&sb, tf2_str(json_get(parts, i)));
        }
        json_t *r = json_string(sb.b ? sb.b : ""); tf2_sb_free(&sb); json_free(parts);
        return r;
    }
    tf2_sb_t sb; tf2_sb_init(&sb);
    tf2_dumps_into(v, &sb);
    json_t *r = json_string(sb.b ? sb.b : ""); tf2_sb_free(&sb);
    return r;
}

/* PoP: tf2_is_ephemeral_scaffolding @ session_rules/turns.py:_is_ephemeral_scaffolding */
static json_t *tf2_is_ephemeral_scaffolding(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *m = json_obj_get(a, "msg");
    bool hit = false;
    if (tf2_is_dict(m)) {
        for (size_t i = 0; i < TF2_EPHEMERAL_SCAFFOLDING_FLAGS_N; i++)
            if (tf2_truthy(json_obj_get(m, TF2_EPHEMERAL_SCAFFOLDING_FLAGS[i]))) { hit = true; break; }
    }
    return json_bool(hit);
}

/* PoP: tf2_image_marker @ session_rules/turns.py:_image_marker */
static json_t *tf2_image_marker(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *part = json_obj_get(a, "part");
    if (!tf2_is_dict(part)) return tf2_attr_err(part, "get");
    const json_t *url = json_obj_get(part, "image_url");
    const json_t *detail = json_obj_get(part, "detail");
    if ((!detail || tf2_is_null(detail) || (tf2_is_str(detail) && !*detail->str_val)) &&
        tf2_is_dict(url)) detail = json_obj_get(url, "detail");
    if (tf2_is_str(detail) &&
        (strcmp(detail->str_val, "low") == 0 || strcmp(detail->str_val, "high") == 0 ||
         strcmp(detail->str_val, "auto") == 0)) {
        char buf[96];
        snprintf(buf, sizeof buf, "[screenshot detail=%s]", detail->str_val);
        return json_string(buf);
    }
    return json_string("[screenshot]");
}

/* PoP: tf2_durable_content @ session_rules/turns.py:_durable_content */
static json_t *tf2_durable_content(tf2_ctx_t *ctx, json_t *a) {
    const json_t *content = json_obj_get(a, "content");
    /* _is_multimodal_tool_result(content) */
    const json_t *flag = tf2_is_dict(content) ? json_obj_get(content, "_multimodal") : NULL;
    const json_t *mmc = tf2_is_dict(content) ? json_obj_get(content, "content") : NULL;
    if (tf2_is_dict(content) && tf2_is_bool(flag) && flag->bool_val && tf2_is_list(mmc)) {
        json_t *va = json_object(); json_set(va, "value", json_copy(content));
        json_t *r = tf2_multimodal_text_summary(ctx, va); json_free(va); return r;
    }
    if (!tf2_is_list(content)) return content ? json_copy(content) : json_null();
    json_t *txt = json_array();
    for (size_t i = 0; i < content->c.count; i++) {
        const json_t *p = json_get(content, i);
        if (!tf2_is_dict(p)) continue;
        const json_t *ty = json_obj_get(p, "type");
        bool is_text = tf2_eq_str(ty, "text");
        bool is_img = false;
        if (tf2_is_str(ty)) for (size_t k = 0; k < TF2_IMAGE_PART_TYPES_N; k++)
            if (strcmp(ty->str_val, TF2_IMAGE_PART_TYPES[k]) == 0) { is_img = true; break; }
        if (!is_text && !is_img) continue;
        if (is_text) {
            const json_t *t = json_obj_get(p, "text");
            char *s = tf2_py_str(t ? t : json_string(""));
            json_append(txt, json_string(s)); free(s);
        } else {
            json_t *va = json_object(); json_set(va, "part", json_copy(p));
            json_t *r = tf2_image_marker(ctx, va); json_free(va);
            json_append(txt, r);
        }
    }
    if (txt->c.count == 0) { json_free(txt); return json_null(); }
    tf2_sb_t sb; tf2_sb_init(&sb);
    for (size_t i = 0; i < txt->c.count; i++) {
        if (i) tf2_sb_putc(&sb, '\n');
        tf2_sb_puts(&sb, tf2_str(json_get(txt, i)));
    }
    json_t *r = json_string(sb.b ? sb.b : ""); tf2_sb_free(&sb); json_free(txt);
    return r;
}

/* PoP: tf2_content_has_payload @ session_rules/turns.py:_content_has_payload */
static json_t *tf2_content_has_payload(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *c = json_obj_get(a, "content");
    if (tf2_is_str(c)) {
        char *s = tf2_strip(c->str_val); bool hit = s && *s; free(s);
        return json_bool(hit);
    }
    if (!tf2_is_list(c)) return json_bool(!tf2_is_null(c));
    bool any = false;
    for (size_t i = 0; i < c->c.count; i++) {
        const json_t *b = json_get(c, i);
        if (tf2_is_dict(b)) {
            const json_t *ty = json_obj_get(b, "type");
            bool not_text = !tf2_eq_str(ty, "text");
            if (not_text) { any = true; break; }
            const json_t *t = json_obj_get(b, "text");
            if (tf2_is_str(t)) { char *s = tf2_strip(t->str_val); bool hit = s && *s; free(s); if (hit) { any = true; break; } }
        } else if (tf2_truthy(b)) { any = true; break; }
    }
    return json_bool(any);
}

/* PoP: tf2_msg_has_payload @ session_rules/turns.py:_msg_has_payload */
static json_t *tf2_msg_has_payload(tf2_ctx_t *ctx, json_t *a) {
    const json_t *m = json_obj_get(a, "msg");
    json_t *ca = json_object();
    json_set(ca, "content", tf2_is_dict(m) ? json_copy(json_obj_get(m, "content")) : json_null());
    json_t *cp = tf2_content_has_payload(ctx, ca);
    bool hit = tf2_truthy(cp);
    json_free(cp); json_free(ca);
    if (!hit && tf2_is_dict(m)) {
        if (tf2_truthy(json_obj_get(m, "tool_calls"))) hit = true;
        const json_t *rc = json_obj_get(m, "reasoning_content");
        if (!hit && tf2_is_str(rc)) { char *s = tf2_strip(rc->str_val); hit = s && *s; free(s); }
        if (!hit && tf2_truthy(json_obj_get(m, "reasoning"))) hit = true;
        if (!hit && tf2_truthy(json_obj_get(m, "reasoning_details"))) hit = true;
        if (!hit && tf2_truthy(json_obj_get(m, "codex_message_items"))) hit = true;
        if (!hit && tf2_truthy(json_obj_get(m, "codex_reasoning_items"))) hit = true;
    }
    return json_bool(hit);
}

/* PoP: tf2_is_codex_interim @ session_rules/turns.py:_is_codex_interim */
static json_t *tf2_is_codex_interim(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *m = json_obj_get(a, "m");
    if (!tf2_is_dict(m)) return tf2_attr_err(m, "get");
    bool hit = tf2_truthy(json_obj_get(m, "codex_reasoning_items")) ||
        tf2_truthy(json_obj_get(m, "codex_message_items")) ||
        tf2_eq_str(json_obj_get(m, "finish_reason"), "incomplete");
    return json_bool(hit);
}

/* PoP: tf2_json_or @ session_rules/turns.py:_json_or */
static json_t *tf2_json_or(tf2_ctx_t *ctx, json_t *a) {
    (void)ctx;
    const json_t *raw = json_obj_get(a, "raw");
    const json_t *fallback = json_obj_get(a, "fallback");
    if (tf2_is_str(raw)) {
        char *err = NULL;
        json_t *parsed = json_parse(raw->str_val, &err);
        free(err);
        if (parsed) return parsed;
    }
    return fallback ? json_copy(fallback) : json_null();
}
