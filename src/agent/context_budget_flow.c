/* context_budget_flow.c — FLOW + leaf port of the context-window budget
 * planning cluster. The task names agent/context_budget.py, which does NOT
 * exist in the quarry; the real defining module for context budget (window)
 * management is agent/context_compressor_window.py (CompactionWindowMixin +
 * summary_view / _restart_probe_extra). PoP annotations credit the REAL
 * defining modules:
 *   - agent/context_compressor_window.py (9 unclaimed FLOW defs)
 *   - agent/context_compressor_tail.py:tail_cut_progress_floor (leaf FLOW)
 *   - agent/context_compressor.py:_tool_calls_by_id / _tc_get / _rewritten
 *   - session_rules/media.py:archive_refs_in_text (pure regex leaf)
 *
 * Claimed siblings are SEAMED through the cbg_io_t universal seam (they are
 * credited elsewhere under agent/context_compressor.py): _protect_head_size,
 * _effective_protect_first_n, _restart_handoff_probe_bounds,
 * _align_boundary_forward, _align_boundary_backward, _find_tail_cut_by_tokens,
 * _find_last_user_message_idx, _is_context_summary_message,
 * _prune_old_tool_results, and session_rules/turns.py:drop_stale_api_content.
 * Heavy unclaimed collaborators (_drop_blank_echoes, _demote_tool_result_at,
 * _demote_stale_tail_tools, _assemble_compressed) and all foreign I/O
 * (tools.tool_result_storage archive/backend functions, prepared_compaction
 * adopt/take/stop, logger) are scripted seams too. Ported siblings compose for
 * REAL (no seam event of their own; their inner seams are traced).
 *
 * Self model: in.state mirrors the CompactionWindowMixin instance attributes.
 * The oracle drives the LIVE extracted mixin class, so direct attribute reads
 * raise AttributeError naming 'CompactionWindowMixin'; class defaults
 * prepared_compaction=None and pin_task_turn=False apply when the key is
 * absent. getattr(self, k, default) sites use the default. No ported def
 * writes a self attribute, so out-state echoes in.state verbatim.
 *
 * In-function imports of CONSTANTS fire one "import_const" seam
 * {module, name} (the oracle stub module's __getattr__ records the read);
 * imports of ported functions compose inline; imports of seamed functions
 * bind recorders silently (the CALL fires the seam). import_const scripts
 * must not raise AttributeError (IMPORT_FROM would convert it to ImportError;
 * excluded domain).
 *
 * Seam contracts (documented for the generator):
 *   _prune_old_tool_results -> [working, pruned_count] (2-list)
 *   archive_compaction_drops / archive_pruned_originals -> [rows, written, kept]
 *   _demote_tool_result_at -> {"ret": bool, "row": <replacement row>} protocol:
 *     when ret is true the row REPLACES result[idx] on both sides (live always
 *     replaces the row when it returns True); "row" required when ret true.
 *     A plain bool/null result is also accepted (no mutation).
 *   pruned_archive_refs -> array of ref strings (live set; the trace carries
 *     the script order, the consumer sorts+uniques before archive_list_for_summary)
 *   take / _assemble_compressed / _drop_blank_echoes / _demote_stale_tail_tools
 *     -> plain records/arrays (no __attr__ wrappers in seam RESULTS)
 *   drop_stale_api_content -> null; the stub pops api_content from the passed
 *     msg (C models the pop AFTER the seam call, args carry the pre-pop row)
 *   logger.warning -> {fmt, args:[failed]} (exact %-style lazy format)
 *
 * Decision sequences only: Python truthiness, `or` short-circuits, `is None`
 * identity, kwarg/operand evaluation order, dict insertion order, slice
 * clamping, and exception scopes are transcribed per call site. None of the
 * 14 defs has a try/except, so EVERY scripted raise (incl. the BaseException
 * trio SystemExit / KeyboardInterrupt / GeneratorExit and CancelledError)
 * propagates past every guard to the top.
 *
 * Value model (shared with tests/oracle_context_budget_flow.py):
 *   - attribute doubles: {"__attr__": {...}} <-> types.SimpleNamespace
 *   - raise specs: {"__raise__": "<ClassName>", "message": str(e)}
 *   - rows / payloads: plain JSON objects (isinstance(x, dict) is True ONLY
 *     for a plain object, never an __attr__ double)
 *   - integral floats normalize to ints on both sides.
 *
 * Domain (ASCII scope, documented for the generator):
 *   - messages / plan / originals / texts are JSON arrays; row elements are
 *     plain objects except where a .get AttributeError case is intended.
 *   - tool_call ids / tool names are strings (or falsy -> ""); non-str truthy
 *     ids are outside the dict-key domain (list/dict ids raise unhashable).
 *   - index seams (_align_boundary_*, _find_*, _protect_head_size) return
 *     integral numbers; head_end / start / end / cut_idx are integral >= 0
 *     (summary_view slice bounds may be negative — Python clamping applies).
 *   - _offload_window_results range bounds: 0 <= start, end <= len(plan)
 *     (start > end is the empty loop); negative range bounds excluded.
 *   - _tc_get keys are not str-method names (getattr on JSON scalars always
 *     yields the default); prepared / scan doubles carry their read fields
 *     (summary / tail_start) except one pinned absent-attribute case each.
 *   - has_archive_pointer is SEAMED (its pointer separator carries U+2014,
 *     outside the ASCII port scope); archive_refs_in_text patterns are ASCII.
 *   - str() / concatenation positions carry str; non-str operands raise the
 *     transcribed TypeError.
 *
 * Self-contained: libjson only (the gate links lib/libjson/json.o). The
 * harness #includes this .c via the area-qualified
 * "agent/context_budget_flow.c". Every symbol is static cbg_*.
 *
 * Gate: bash tests/run_one_oracle.sh context_budget_flow \
 *           oracle_context_budget_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── live module constants (byte-pinned) ── */
/* agent/context_compressor_window.py: _SUMMARY_KEEPS_PRUNED_FORM */
#define CBG_SUMMARY_KEEPS_PRUNED_FORM "clarify"
/* agent/context_compressor_window.py: _commit_window logger.warning format
 * (implicit two-line concatenation in the live source, byte-pinned). */
#define CBG_ARCHIVE_WARN_FMT \
    "compaction: %d summarized tool result(s) could not be archived (disk full or " \
    "read-only home); only the summary and the session store keep them"
/* tools/tool_result_storage.py: _offload_window_results stub content */
#define CBG_ARCHIVED_STUB "[Tool output archived]"
/* session_rules/media.py: _ARCHIVE_NAME_RES subdirs (dict order: spillover,
 * pruned) and name character class [A-Za-z0-9_.-]. */
#define CBG_SPILLOVER_SUBDIR "cache/spillover"
#define CBG_PRUNED_SUBDIR "cache/pruned"

/* ── universal seam ── */
typedef struct cbg_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cbg_io_t;

/* ── truthiness / accessors ── */
static int cbg_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static int cbg_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int cbg_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int cbg_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int cbg_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static int cbg_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static int cbg_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *cbg_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *cbg_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double cbg_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* isinstance(x, dict): a PLAIN object, never an __attr__ SimpleNamespace. */
static int cbg_is_plain_dict(const json_t *v)
{
    return cbg_is_obj(v) && json_obj_get(v, "__attr__") == NULL;
}

/* getattr on an attribute double; any other carrier has no attributes. */
static const json_t *cbg_getattr(const json_t *v, const char *name)
{
    if (!cbg_is_obj(v)) return NULL;
    const json_t *inner = json_obj_get(v, "__attr__");
    if (!inner || inner->type != JSON_OBJECT) return NULL;
    return json_obj_get(inner, name);
}

static const char *cbg_typename(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == (double)(long long)d && fabs(d) < 9.2e18) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT:
            return json_obj_get(v, "__attr__") ? "types.SimpleNamespace" : "dict";
        default: return "object";
    }
}

/* ── raise specs ── */
static int cbg_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *cbg_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static char *cbg_xfmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

static json_t *cbg_attr_raise(const json_t *v, const char *name)
{
    char *m = cbg_xfmt("'%s' object has no attribute '%s'", cbg_typename(v), name);
    json_t *r = cbg_mkraise("AttributeError", m ? m : "");
    free(m);
    return r;
}

/* direct self.<name> read on the mixin double (class has no such attribute) */
static json_t *cbg_self_raise(const char *name)
{
    char *m = cbg_xfmt("'CompactionWindowMixin' object has no attribute '%s'", name);
    json_t *r = cbg_mkraise("AttributeError", m ? m : "");
    free(m);
    return r;
}

static json_t *cbg_type_raise(const char *kind, const char *fmt, const json_t *v)
{
    char *m = cbg_xfmt(fmt, cbg_typename(v));
    json_t *r = cbg_mkraise(kind, m ? m : "");
    free(m);
    return r;
}

/* ── universal seam call: args OWNED by caller, borrowed by dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. ── */
static json_t *cbg_call(cbg_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cbg_xfmt("no seam installed for %s", name);
        r = cbg_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *cbg_args(void) { return json_object(); }

/* arg builders: None-valued keys dropped on both sides (harness strips nulls
 * recursively; oracle norm_args drops None). */
static void cbg_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void cbg_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void cbg_arg_b(json_t *a, const char *k, int v)
{
    json_set(a, k, json_bool(v ? true : false));
}
static void cbg_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* ── Python slice clamping: list[a:b] over a length-n sequence ── */
static void cbg_slice_bounds(double n, double a, double b, size_t *oa, size_t *ob)
{
    if (a < 0) a += n;
    if (b < 0) b += n;
    if (a < 0) a = 0;
    if (b < 0) b = 0;
    if (a > n) a = n;
    if (b > n) b = n;
    *oa = (size_t)a;
    *ob = (size_t)b;
}

/* len(x) for the array domain; non-array raises the live TypeError. */
static size_t cbg_len(const json_t *v, json_t **err)
{
    *err = NULL;
    if (cbg_is_arr(v) || cbg_is_obj(v)) return v->c.count;
    if (cbg_is_str(v)) return v->str_val ? strlen(v->str_val) : 0;
    char *m = cbg_xfmt("object of type '%s' has no len()", cbg_typename(v));
    *err = cbg_mkraise("TypeError", m ? m : "");
    free(m);
    return 0;
}

/* messages[i] with i >= 0: IndexError when out of range (no negative domain). */
static const json_t *cbg_index(const json_t *arr, double i, json_t **err)
{
    *err = NULL;
    if (!cbg_is_arr(arr) || i < 0 || (size_t)i >= arr->c.count) {
        *err = cbg_mkraise("IndexError", "list index out of range");
        return NULL;
    }
    return arr->c.items[(size_t)i];
}

/* row.get(key): non-dict row raises AttributeError naming the carrier type. */
static const json_t *cbg_row_get(const json_t *row, const char *key, json_t **err)
{
    *err = NULL;
    if (!cbg_is_plain_dict(row)) {
        *err = cbg_attr_raise(row, "get");
        return NULL;
    }
    return json_obj_get(row, key);
}

/* str == str comparison (NULL-safe; non-str never equals a C literal). */
static int cbg_str_eq(const json_t *v, const char *lit)
{
    const char *s = cbg_str(v);
    return s && strcmp(s, lit) == 0;
}

/* ── sorted-unique string array (live set normalization) ── */
static int cbg_str_cmp(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Collect the string elements of arr into a sorted, deduped JSON array.
 * Non-string elements are outside the ref-set domain (fail closed). */
static json_t *cbg_sorted_unique(const json_t *arr, json_t **err)
{
    *err = NULL;
    size_t n = cbg_is_arr(arr) ? arr->c.count : 0;
    char **tmp = n ? (char **)malloc(n * sizeof(char *)) : NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = cbg_str(arr->c.items[i]);
        if (!s) {
            *err = cbg_type_raise("TypeError", "unhashable type: '%s'", arr->c.items[i]);
            for (size_t j = 0; j < k; j++) free(tmp[j]);
            free(tmp);
            return NULL;
        }
        tmp[k++] = strdup(s);
    }
    if (k > 1) qsort(tmp, k, sizeof(char *), cbg_str_cmp);
    json_t *out = json_array();
    for (size_t i = 0; i < k; i++) {
        if (i == 0 || strcmp(tmp[i], tmp[i - 1]) != 0)
            json_append(out, json_string(tmp[i]));
        free(tmp[i]);
    }
    free(tmp);
    return out;
}

/* ── unpack helpers: live `a, b = seq` / `a, b, c = seq` ── */
static void cbg_unpack(const json_t *seq, size_t want, json_t ***items, json_t **err)
{
    *err = NULL;
    *items = NULL;
    if (cbg_is_null(seq)) {
        *err = cbg_type_raise("TypeError", "'%s' object is not iterable", seq);
        return;
    }
    if (!cbg_is_arr(seq)) {
        /* str iterates chars / dict iterates keys: outside the scripted domain;
         * numbers and other scalars raise the live not-iterable TypeError. */
        *err = cbg_type_raise("TypeError", "'%s' object is not iterable", seq);
        return;
    }
    size_t got = seq->c.count;
    if (got != want) {
        char *m = cbg_xfmt("%s values to unpack (expected %zu, got %zu)",
                           got < want ? "not enough" : "too many", want, got);
        *err = cbg_mkraise("ValueError", m ? m : "");
        free(m);
        return;
    }
    json_t **it = (json_t **)malloc(want * sizeof(json_t *));
    for (size_t i = 0; i < want; i++) it[i] = json_copy(seq->c.items[i]);
    *items = it;
}

static void cbg_unpack_free(json_t **items, size_t n)
{
    if (!items) return;
    for (size_t i = 0; i < n; i++) json_free(items[i]);
    free(items);
}

/* ── numeric arithmetic with the live TypeError on non-numbers ── */
static json_t *cbg_arith(const json_t *a, const json_t *b, char op, json_t **err)
{
    *err = NULL;
    if (!cbg_is_num(a) && !cbg_is_bool(a)) {
        char *m = cbg_xfmt("unsupported operand type(s) for %c: '%s' and '%s'",
                           op, cbg_typename(a), cbg_typename(b));
        *err = cbg_mkraise("TypeError", m ? m : "");
        free(m);
        return NULL;
    }
    if (!cbg_is_num(b) && !cbg_is_bool(b)) {
        char *m = cbg_xfmt("unsupported operand type(s) for %c: '%s' and '%s'",
                           op, cbg_typename(a), cbg_typename(b));
        *err = cbg_mkraise("TypeError", m ? m : "");
        free(m);
        return NULL;
    }
    double x = cbg_is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
    double y = cbg_is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
    return json_number(op == '-' ? x - y : x + y);
}

/* ── _tc_get core: dict .get / SimpleNamespace getattr / scalar default ── */
static const json_t *cbg_tc_field(const json_t *obj, const char *key, const json_t *def)
{
    if (cbg_is_plain_dict(obj)) {
        const json_t *v = json_obj_get(obj, key);
        return v ? v : def;
    }
    if (cbg_is_obj(obj)) {           /* __attr__ double: getattr */
        const json_t *v = cbg_getattr(obj, key);
        return v ? v : def;
    }
    return def;                      /* JSON scalars: getattr default */
}

/* ── archive_refs_in_text scanners (session_rules/media.py, ASCII) ──
 * spillover: r"spillover(?:/|\\+)([A-Za-z0-9_.-]+)"
 * pruned:    r"cache/pruned/([A-Za-z0-9_.-]+)"
 * findall: non-overlapping, left-to-right; a failed attempt advances 1. */
static int cbg_name_char(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
        || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
}

typedef struct { char **v; size_t n, cap; } cbg_strvec;

static void cbg_sv_push(cbg_strvec *sv, const char *s)
{
    if (sv->n == sv->cap) {
        sv->cap = sv->cap ? sv->cap * 2 : 8;
        sv->v = (char **)realloc(sv->v, sv->cap * sizeof(char *));
    }
    sv->v[sv->n++] = strdup(s);
}

static void cbg_sv_free(cbg_strvec *sv)
{
    for (size_t i = 0; i < sv->n; i++) free(sv->v[i]);
    free(sv->v);
    sv->v = NULL; sv->n = sv->cap = 0;
}

static void cbg_scan_spillover(const char *s, cbg_strvec *refs)
{
    size_t n = strlen(s), p = 0;
    while (p < n) {
        if (strncmp(s + p, "spillover", 9) != 0) { p++; continue; }
        size_t q = p + 9;
        if (q < n && s[q] == '/') q++;
        else if (q < n && s[q] == '\\') { while (q < n && s[q] == '\\') q++; }
        else { p++; continue; }
        size_t ns = q;
        while (q < n && cbg_name_char((unsigned char)s[q])) q++;
        if (q == ns) { p++; continue; }
        char *ref = cbg_xfmt("%s/%.*s", CBG_SPILLOVER_SUBDIR, (int)(q - ns), s + ns);
        if (ref) { cbg_sv_push(refs, ref); free(ref); }
        p = q;
    }
}

static void cbg_scan_pruned(const char *s, cbg_strvec *refs)
{
    size_t n = strlen(s), p = 0;
    while (p < n) {
        if (strncmp(s + p, "cache/pruned/", 13) != 0) { p++; continue; }
        size_t q = p + 13, ns = q;
        while (q < n && cbg_name_char((unsigned char)s[q])) q++;
        if (q == ns) { p++; continue; }
        char *ref = cbg_xfmt("%s/%.*s", CBG_PRUNED_SUBDIR, (int)(q - ns), s + ns);
        if (ref) { cbg_sv_push(refs, ref); free(ref); }
        p = q;
    }
}

/* ════════════════════════ PORTED LEAVES ════════════════════════ */

/* PoP: _tc_get @ agent/context_compressor.py:_tc_get */
static json_t *cbg_tc_get(cbg_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *obj = cbg_get(in, "obj");
    const char *key = cbg_str(cbg_get(in, "key"));
    const json_t *def = cbg_get(in, "default");
    json_t *defcopy = def ? json_copy(def) : json_string("");
    if (!key) return defcopy;      /* non-str key outside domain */
    const json_t *v = cbg_tc_field(obj, key, NULL);
    if (!v) return defcopy;
    json_free(defcopy);
    return json_copy(v);
}

/* PoP: _tool_calls_by_id @ agent/context_compressor.py:_tool_calls_by_id */
static json_t *cbg_tool_calls_by_id(cbg_io_t *io, const json_t *messages, json_t **err);

static json_t *cbg_tool_calls_by_id_entry(cbg_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    json_t *err = NULL;
    json_t *r = cbg_tool_calls_by_id(io, cbg_get(in, "messages"), &err);
    return err ? err : r;
}

static json_t *cbg_tool_calls_by_id(cbg_io_t *io, const json_t *messages, json_t **err)
{
    (void)io;
    *err = NULL;
    if (!cbg_is_arr(messages)) {
        *err = cbg_type_raise("TypeError", "'%s' object is not iterable", messages);
        return NULL;
    }
    json_t *out = json_object();
    json_t *empty_def = json_object();
    json_t *str_def = json_string("");
    json_t *unknown_def = json_string("unknown");
    for (size_t i = 0; i < messages->c.count; i++) {
        const json_t *msg = messages->c.items[i];
        json_t *ge = NULL;
        const json_t *role = cbg_row_get(msg, "role", &ge);
        if (ge) { json_free(out); json_free(empty_def); json_free(str_def); json_free(unknown_def); *err = ge; return NULL; }
        if (!cbg_str_eq(role, "assistant")) continue;
        const json_t *tcs = json_obj_get(msg, "tool_calls");
        if (!cbg_truth(tcs)) continue;          /* `or []` -> empty loop */
        if (!cbg_is_arr(tcs)) {
            /* non-list truthy tool_calls iterates chars/keys: excluded domain */
            json_free(out); json_free(empty_def); json_free(str_def); json_free(unknown_def);
            *err = cbg_type_raise("TypeError", "'%s' object is not iterable", tcs);
            return NULL;
        }
        for (size_t j = 0; j < tcs->c.count; j++) {
            const json_t *tc = tcs->c.items[j];
            const json_t *fn = cbg_tc_field(tc, "function", empty_def);
            const json_t *idv = cbg_tc_field(tc, "id", str_def);
            const char *key = "";
            if (cbg_truth(idv)) {
                const char *ids = cbg_str(idv);
                if (!ids) {
                    /* truthy non-str id: list/dict unhashable; int/float/bool
                     * keys are outside the str-key domain (fail closed). */
                    json_free(out); json_free(empty_def); json_free(str_def); json_free(unknown_def);
                    *err = cbg_type_raise("TypeError", "unhashable type: '%s'", idv);
                    return NULL;
                }
                key = ids;
            }
            const json_t *name = cbg_tc_field(fn, "name", unknown_def);
            const json_t *args = cbg_tc_field(fn, "arguments", str_def);
            json_t *pair = json_array();
            json_append(pair, name ? json_copy(name) : json_null());
            json_append(pair, args ? json_copy(args) : json_null());
            json_set(out, key, pair);
        }
    }
    json_free(empty_def); json_free(str_def); json_free(unknown_def);
    return out;
}

/* _rewritten core (composed from _offload_window_results and the direct entry):
 * new_msg = {**msg, "content": content}; drop_stale_api_content(new_msg). */
static json_t *cbg_rewritten_core(cbg_io_t *io, const json_t *msg, const json_t *content)
{
    if (!cbg_is_plain_dict(msg))
        return cbg_type_raise("TypeError", "'%s' object is not a mapping", msg);
    json_t *new_msg = json_copy(msg);
    json_set(new_msg, "content", content ? json_copy(content) : json_null());
    json_t *a = cbg_args();
    cbg_arg_j(a, "msg", new_msg);
    json_t *r = cbg_call(io, "drop_stale_api_content", a);
    if (cbg_is_raise(r)) { json_free(new_msg); return r; }
    json_free(r);
    json_obj_del(new_msg, "api_content");   /* live stub pop, after the call */
    return new_msg;
}

/* PoP: _rewritten @ agent/context_compressor.py:_rewritten */
static json_t *cbg_rewritten(cbg_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return cbg_rewritten_core(io, cbg_get(in, "msg"), cbg_get(in, "content"));
}

/* PoP: archive_refs_in_text @ session_rules/media.py:archive_refs_in_text */
static json_t *cbg_archive_refs_in_text(cbg_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *texts = cbg_get(in, "texts");
    if (!cbg_is_arr(texts))
        return cbg_type_raise("TypeError", "'%s' object is not iterable", texts);
    cbg_strvec refs = {0};
    for (size_t i = 0; i < texts->c.count; i++) {
        const char *s = cbg_str(texts->c.items[i]);
        if (!s) continue;                 /* isinstance(text, str) gate */
        cbg_scan_spillover(s, &refs);     /* _ARCHIVE_NAME_RES dict order */
        cbg_scan_pruned(s, &refs);
    }
    if (refs.n > 1) qsort(refs.v, refs.n, sizeof(char *), cbg_str_cmp);
    json_t *out = json_array();
    for (size_t i = 0; i < refs.n; i++) {
        if (i == 0 || strcmp(refs.v[i], refs.v[i - 1]) != 0)
            json_append(out, json_string(refs.v[i]));
    }
    cbg_sv_free(&refs);
    return out;
}

/* PoP: tail_cut_progress_floor @ agent/context_compressor_tail.py:tail_cut_progress_floor */
static json_t *cbg_tail_cut_progress_floor(cbg_io_t *io, json_t *st,
                                           const json_t *messages,
                                           double head_end, double cut_idx);

static json_t *cbg_tail_cut_progress_floor_entry(cbg_io_t *io, const json_t *in, json_t *st)
{
    return cbg_tail_cut_progress_floor(io, st, cbg_get(in, "messages"),
                                       cbg_num(cbg_get(in, "head_end")),
                                       cbg_num(cbg_get(in, "cut_idx")));
}

static json_t *cbg_tail_cut_progress_floor(cbg_io_t *io, json_t *st,
                                           const json_t *messages,
                                           double head_end, double cut_idx)
{
    (void)st;
    double floor_v = head_end + 1;
    size_t n = cbg_is_arr(messages) ? messages->c.count : 0;
    double newest = -1;
    /* next((i for i in range(len-1, head_end-1, -1) if seam(messages[i])), -1)
     * head_end >= 0 domain: the reverse scan stops at head_end. */
    for (double i = (double)n - 1; i > head_end - 1; i -= 1) {
        if (i < 0) break;
        json_t *a = cbg_args();
        cbg_arg_j(a, "msg", messages->c.items[(size_t)i]);
        json_t *r = cbg_call(io, "_is_context_summary_message", a);
        if (cbg_is_raise(r)) return r;
        int t = cbg_truth(r);
        json_free(r);
        if (t) { newest = i; break; }
    }
    if (newest < 0) return json_number(floor_v);
    /* all(map(seam, messages[head_end:max(cut_idx, newest+1)])) */
    double bmax = cut_idx > newest + 1 ? cut_idx : newest + 1;
    size_t a, b;
    cbg_slice_bounds((double)n, head_end, bmax, &a, &b);
    int all_true = 1;
    for (size_t i = a; i < b; i++) {
        json_t *ar = cbg_args();
        cbg_arg_j(ar, "msg", messages->c.items[i]);
        json_t *r = cbg_call(io, "_is_context_summary_message", ar);
        if (cbg_is_raise(r)) return r;
        int t = cbg_truth(r);
        json_free(r);
        if (!t) { all_true = 0; break; }
    }
    if (!all_true) return json_number(floor_v);
    json_t *a1 = cbg_args();
    cbg_arg_j(a1, "messages", messages);
    cbg_arg_n(a1, "idx", newest + 2);
    json_t *raised_r = cbg_call(io, "_align_boundary_forward", a1);
    if (cbg_is_raise(raised_r)) return raised_r;
    double raised = cbg_num(raised_r);
    json_free(raised_r);
    json_t *a2 = cbg_args();
    cbg_arg_j(a2, "messages", messages);
    cbg_arg_n(a2, "start", newest + 1);
    json_t *lu_r = cbg_call(io, "_find_last_user_message_idx", a2);
    if (cbg_is_raise(lu_r)) return lu_r;
    double last_user = cbg_num(lu_r);
    json_free(lu_r);
    if (raised >= (double)n || (last_user >= 0 && last_user < raised))
        return json_number(floor_v);
    return json_number(raised);
}

/* ════════════════════════ PORTED FLOWS ════════════════════════ */

/* PoP: _restart_probe_extra @ agent/context_compressor_window.py:_restart_probe_extra */
static json_t *cbg_restart_probe_extra(cbg_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *a = cbg_args();
    cbg_arg_s(a, "module", "agent.context_compressor");
    cbg_arg_s(a, "name", "_RESTART_HANDOFF_PROBE_EXTRA_MESSAGES");
    return cbg_call(io, "import_const", a);
}

/* PoP: summary_view @ agent/context_compressor_window.py:summary_view */
static json_t *cbg_summary_view(cbg_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *plan = cbg_get(in, "plan");
    const json_t *originals = cbg_get(in, "originals");
    json_t *err = NULL;
    size_t no = cbg_len(originals, &err);   /* live evaluates len(originals) first */
    if (err) return err;
    size_t np = cbg_len(plan, &err);
    if (err) return err;
    if (no != np) return json_copy(plan);
    json_t *calls = cbg_tool_calls_by_id(io, originals, &err);
    if (err) return err;
    double start = cbg_num(cbg_get(in, "start"));
    double end = cbg_num(cbg_get(in, "end"));
    size_t a, b;
    cbg_slice_bounds((double)np, start, end, &a, &b);
    json_t *window = json_array();
    for (size_t i = a; i < b; i++) {
        const json_t *pruned = plan->c.items[i];
        const json_t *original = originals->c.items[i];
        json_t *ge = NULL;
        const json_t *role = cbg_row_get(original, "role", &ge);
        if (ge) { json_free(window); json_free(calls); return ge; }
        int keep_original = 0;
        if (cbg_str_eq(role, "tool")) {
            const json_t *tcid = json_obj_get(original, "tool_call_id");
            const json_t *entry = NULL;
            if (cbg_truth(tcid)) {
                const char *key = cbg_str(tcid);
                if (key) entry = json_obj_get(calls, key);
                else if (cbg_is_arr(tcid) || cbg_is_obj(tcid)) {
                    /* unhashable dict.get key */
                    json_free(window); json_free(calls);
                    return cbg_type_raise("TypeError", "unhashable type: '%s'", tcid);
                }
                /* integral id: hashable but never a str key -> miss */
            }
            const json_t *name = NULL;
            if (entry && cbg_is_arr(entry) && entry->c.count > 0)
                name = entry->c.items[0];
            /* name not in _SUMMARY_KEEPS_PRUNED_FORM (frozenset {"clarify"}) */
            if (name && (cbg_is_arr(name) || cbg_is_obj(name))) {
                json_free(window); json_free(calls);
                char *m = cbg_xfmt("cannot use '%s' as a set element (unhashable type: '%s')",
                                   cbg_typename(name), cbg_typename(name));
                json_t *r = cbg_mkraise("TypeError", m ? m : "");
                free(m);
                return r;
            }
            keep_original = !cbg_str_eq(name, CBG_SUMMARY_KEEPS_PRUNED_FORM);
        }
        json_append(window, json_copy(keep_original ? original : pruned));
    }
    json_free(calls);
    /* [*plan[:start], *window, *plan[end:]] */
    json_t *out = json_array();
    size_t ha, hb;
    cbg_slice_bounds((double)np, 0, start, &ha, &hb);
    for (size_t i = ha; i < hb; i++) json_append(out, json_copy(plan->c.items[i]));
    for (size_t i = 0; i < window->c.count; i++) json_append(out, json_copy(window->c.items[i]));
    cbg_slice_bounds((double)np, end, (double)np, &ha, &hb);
    for (size_t i = ha; i < hb; i++) json_append(out, json_copy(plan->c.items[i]));
    json_free(window);
    return out;
}

/* PoP: _compress_window @ agent/context_compressor_window.py:_compress_window */
static json_t *cbg_compress_window(cbg_io_t *io, json_t *st, const json_t *messages);

static json_t *cbg_compress_window_entry(cbg_io_t *io, const json_t *in, json_t *st)
{
    return cbg_compress_window(io, st, cbg_get(in, "messages"));
}

static json_t *cbg_compress_window(cbg_io_t *io, json_t *st, const json_t *messages)
{
    /* compress_start = self._align_boundary_forward(messages,
     *                     self._protect_head_size(messages)) */
    json_t *a0 = cbg_args();
    cbg_arg_j(a0, "messages", messages);
    json_t *head_r = cbg_call(io, "_protect_head_size", a0);
    if (cbg_is_raise(head_r)) return head_r;
    double head = cbg_num(head_r);
    json_free(head_r);
    json_t *a1 = cbg_args();
    cbg_arg_j(a1, "messages", messages);
    cbg_arg_n(a1, "idx", head);
    json_t *cs_r = cbg_call(io, "_align_boundary_forward", a1);
    if (cbg_is_raise(cs_r)) return cs_r;
    double compress_start = cbg_num(cs_r);
    json_free(cs_r);
    json_t *a2 = cbg_args();
    cbg_arg_j(a2, "messages", messages);
    cbg_arg_n(a2, "start", compress_start);
    json_t *ce_r = cbg_call(io, "_find_tail_cut_by_tokens", a2);
    if (cbg_is_raise(ce_r)) return ce_r;
    double compress_end = cbg_num(ce_r);
    json_free(ce_r);
    json_t *a3 = cbg_args();
    cbg_arg_j(a3, "messages", messages);
    cbg_arg_n(a3, "start", 0);
    json_t *lu_r = cbg_call(io, "_find_last_user_message_idx", a3);
    if (cbg_is_raise(lu_r)) return lu_r;
    double latest = cbg_num(lu_r);
    json_free(lu_r);
    if (compress_end == latest) {
        double bridge_idx = latest - 1;
        const json_t *bridge_role = NULL;
        if (bridge_idx >= 0) {
            json_t *ie = NULL;
            const json_t *elem = cbg_index(messages, bridge_idx, &ie);
            if (ie) return ie;
            json_t *ge = NULL;
            bridge_role = cbg_row_get(elem, "role", &ge);
            if (ge) return ge;
        }
        if (cbg_str_eq(bridge_role, "tool")) {
            json_t *a4 = cbg_args();
            cbg_arg_j(a4, "messages", messages);
            cbg_arg_n(a4, "idx", latest);
            json_t *bb_r = cbg_call(io, "_align_boundary_backward", a4);
            if (cbg_is_raise(bb_r)) return bb_r;
            bridge_idx = cbg_num(bb_r);
            json_free(bb_r);
        } else if (!cbg_str_eq(bridge_role, "assistant")) {
            bridge_idx = -1;
        }
        json_t *floor_r = cbg_tail_cut_progress_floor(io, st, messages,
                                                      compress_start, bridge_idx);
        if (cbg_is_raise(floor_r)) return floor_r;
        double floor_v = cbg_num(floor_r);
        json_free(floor_r);
        if (bridge_idx >= floor_v) compress_end = bridge_idx;
    }
    json_t *out = json_array();
    json_append(out, json_number(compress_start));
    json_append(out, json_number(compress_end));
    return out;
}

/* PoP: _plan_compaction_window @ agent/context_compressor_window.py:_plan_compaction_window */
static json_t *cbg_plan_compaction_window(cbg_io_t *io, const json_t *in, json_t *st)
{
    const json_t *messages = cbg_get(in, "messages");
    /* kwarg evaluation order: protect_tail_count=self.protect_last_n, then
     * protect_tail_tokens=self.tail_token_budget (direct reads, BEFORE call) */
    const json_t *pln = cbg_get(st, "protect_last_n");
    if (!pln) return cbg_self_raise("protect_last_n");
    const json_t *ttb = cbg_get(st, "tail_token_budget");
    if (!ttb) return cbg_self_raise("tail_token_budget");
    json_t *a = cbg_args();
    cbg_arg_j(a, "messages", messages);
    cbg_arg_j(a, "protect_tail_count", pln);
    cbg_arg_j(a, "protect_tail_tokens", ttb);
    json_t *pr = cbg_call(io, "_prune_old_tool_results", a);
    if (cbg_is_raise(pr)) return pr;
    json_t **it = NULL; json_t *ue = NULL;
    cbg_unpack(pr, 2, &it, &ue);
    json_free(pr);
    if (ue) return ue;
    json_t *working = it[0];
    json_t *pruned_count = it[1];
    free(it);
    json_t *a2 = cbg_args();
    cbg_arg_j(a2, "messages", working);
    json_t *plan = cbg_call(io, "_drop_blank_echoes", a2);
    json_free(working);
    if (cbg_is_raise(plan)) { json_free(pruned_count); return plan; }
    json_t *cw = cbg_compress_window(io, st, plan);
    if (cbg_is_raise(cw)) { json_free(plan); json_free(pruned_count); return cw; }
    json_t *out = json_array();
    json_append(out, plan);
    json_append(out, pruned_count);
    json_append(out, json_copy(cw->c.items[0]));
    json_append(out, json_copy(cw->c.items[1]));
    json_free(cw);
    return out;
}

/* _offload_window_results core (composed from _commit_window + direct entry).
 * Returns [rows, stubbed, written, failed] or a raise spec. */
static json_t *cbg_offload_window_results(cbg_io_t *io, json_t *st,
                                          const json_t *plan,
                                          double start, double end)
{
    /* from agent.context_compressor import _PRUNE_MIN_CHARS, _rewritten,
     *      _tool_calls_by_id  (constant read fires import_const; the two
     *      ported functions compose inline, no event) */
    json_t *ac = cbg_args();
    cbg_arg_s(ac, "module", "agent.context_compressor");
    cbg_arg_s(ac, "name", "_PRUNE_MIN_CHARS");
    json_t *min_chars = cbg_call(io, "import_const", ac);
    if (cbg_is_raise(min_chars)) return min_chars;
    /* from tools.tool_result_storage import _backend_reads_host_files,
     *      archive_pruned_originals, has_archive_pointer (silent bind) */
    json_t *bh = cbg_call(io, "_backend_reads_host_files", cbg_args());
    if (cbg_is_raise(bh)) { json_free(min_chars); return bh; }
    int reads = cbg_truth(bh);
    json_free(bh);
    if (!reads) {
        json_free(min_chars);
        json_t *out = json_array();
        json_append(out, json_copy(plan));
        json_append(out, json_number(0));
        json_append(out, json_array());
        json_append(out, json_number(0));
        return out;
    }
    if (!cbg_is_arr(plan)) {
        json_free(min_chars);
        return cbg_type_raise("TypeError", "'%s' object is not iterable", plan);
    }
    json_t *demoted = json_copy(plan);          /* list(plan) */
    json_t *err = NULL;
    json_t *calls = cbg_tool_calls_by_id(io, plan, &err);
    if (err) { json_free(min_chars); json_free(demoted); return err; }
    double changed = 0;
    for (double i = start; i < end; i += 1) {
        json_t *a = cbg_args();
        cbg_arg_j(a, "result", demoted);
        cbg_arg_n(a, "idx", i);
        cbg_arg_j(a, "call_id_to_tool", calls);
        cbg_arg_j(a, "min_prune_chars", min_chars);
        json_t *dr = cbg_call(io, "_demote_tool_result_at", a);
        if (cbg_is_raise(dr)) { json_free(min_chars); json_free(demoted); json_free(calls); return dr; }
        /* {"ret": bool, "row": ...} mutation protocol; plain bool/null too */
        int did = 0;
        const json_t *repl = NULL;
        if (cbg_is_obj(dr) && json_obj_get(dr, "ret")) {
            did = cbg_truth(json_obj_get(dr, "ret"));
            repl = json_obj_get(dr, "row");
        } else {
            did = cbg_truth(dr);
        }
        if (did) {
            if (repl && i >= 0 && (size_t)i < demoted->c.count) {
                json_free(demoted->c.items[(size_t)i]);
                demoted->c.items[(size_t)i] = json_copy(repl);
            }
            changed += 1;
            json_free(dr);
        } else {
            json_free(dr);
            json_t *ie = NULL;
            const json_t *pi = cbg_index(plan, i, &ie);
            if (ie) { json_free(min_chars); json_free(demoted); json_free(calls); return ie; }
            json_t *ge = NULL;
            const json_t *role = cbg_row_get(pi, "role", &ge);
            if (ge) { json_free(min_chars); json_free(demoted); json_free(calls); return ge; }
            if (cbg_str_eq(role, "tool")) {
                const json_t *content = json_obj_get(pi, "content");
                if (cbg_truth(content)) {
                    int demote = 0;
                    if (cbg_is_arr(content) || cbg_is_plain_dict(content)) {
                        demote = 1;
                    } else if (cbg_is_str(content)) {
                        json_t *ha = cbg_args();
                        cbg_arg_j(ha, "content", content);
                        json_t *hp = cbg_call(io, "has_archive_pointer", ha);
                        if (cbg_is_raise(hp)) { json_free(min_chars); json_free(demoted); json_free(calls); return hp; }
                        demote = !cbg_truth(hp);
                        json_free(hp);
                    }
                    if (demote) {
                        json_t *stub = json_string(CBG_ARCHIVED_STUB);
                        json_t *nm = cbg_rewritten_core(io, pi, stub);
                        json_free(stub);
                        if (cbg_is_raise(nm)) { json_free(min_chars); json_free(demoted); json_free(calls); return nm; }
                        if (i >= 0 && (size_t)i < demoted->c.count) {
                            json_free(demoted->c.items[(size_t)i]);
                            demoted->c.items[(size_t)i] = nm;
                        } else {
                            json_free(nm);
                        }
                        changed += 1;
                    }
                }
            }
        }
    }
    json_free(calls);
    if (changed == 0) {
        json_free(min_chars); json_free(demoted);
        json_t *out = json_array();
        json_append(out, json_copy(plan));
        json_append(out, json_number(0));
        json_append(out, json_array());
        json_append(out, json_number(0));
        return out;
    }
    /* getattr(self, "_session_id", "") or "" */
    const json_t *sidv = cbg_get(st, "_session_id");
    const json_t *sid = cbg_truth(sidv) ? sidv : NULL;
    json_t *a2 = cbg_args();
    cbg_arg_j(a2, "messages", plan);
    cbg_arg_j(a2, "pruned", demoted);
    if (sid) cbg_arg_j(a2, "session_id", sid);
    else cbg_arg_s(a2, "session_id", "");
    json_t *ar = cbg_call(io, "archive_pruned_originals", a2);
    json_free(demoted);
    if (cbg_is_raise(ar)) { json_free(min_chars); return ar; }
    json_t **it = NULL; json_t *ue = NULL;
    cbg_unpack(ar, 3, &it, &ue);
    json_free(ar);
    if (ue) { json_free(min_chars); return ue; }
    json_t *rows = it[0], *written = it[1], *kept = it[2];
    free(it);
    json_t *ae = NULL;
    json_t *stubbed = cbg_arith(json_number(changed), kept, '-', &ae);
    if (ae) { json_free(min_chars); json_free(rows); json_free(written); json_free(kept); return ae; }
    json_free(min_chars);
    json_t *out = json_array();
    json_append(out, rows);
    json_append(out, stubbed);
    json_append(out, written);
    json_append(out, kept);   /* live returns (rows, changed-kept, written, kept): failed == kept */
    return out;
}

/* PoP: _offload_window_results @ agent/context_compressor_window.py:_offload_window_results */
static json_t *cbg_offload_window_results_entry(cbg_io_t *io, const json_t *in, json_t *st)
{
    return cbg_offload_window_results(io, st, cbg_get(in, "plan"),
                                      cbg_num(cbg_get(in, "start")),
                                      cbg_num(cbg_get(in, "end")));
}

/* PoP: _commit_window @ agent/context_compressor_window.py:_commit_window */
static json_t *cbg_commit_window(cbg_io_t *io, const json_t *in, json_t *st)
{
    const json_t *originals = cbg_get(in, "originals");
    json_t *plan = json_copy(cbg_get(in, "plan"));
    json_t *pc = json_copy(cbg_get(in, "pruned_count"));
    double start = cbg_num(cbg_get(in, "start"));
    double end = cbg_num(cbg_get(in, "end"));
    /* session_id = getattr(self, "_session_id", "") or "" */
    const json_t *sidv = cbg_get(st, "_session_id");
    const json_t *sid = cbg_truth(sidv) ? sidv : NULL;
    if (cbg_truth(pc)) {
        json_t *a = cbg_args();
        cbg_arg_j(a, "messages", originals);
        cbg_arg_j(a, "dropped", plan);
        if (sid) cbg_arg_j(a, "session_id", sid);
        else cbg_arg_s(a, "session_id", "");
        json_t *ar = cbg_call(io, "archive_compaction_drops", a);
        if (cbg_is_raise(ar)) { json_free(plan); json_free(pc); return ar; }
        json_t **it = NULL; json_t *ue = NULL;
        cbg_unpack(ar, 3, &it, &ue);
        json_free(ar);
        if (ue) { json_free(plan); json_free(pc); return ue; }
        json_free(plan);
        plan = it[0];
        json_t *kept = it[2];
        json_free(it[1]);
        free(it);
        json_t *ae = NULL;
        json_t *npc = cbg_arith(pc, kept, '-', &ae);
        json_free(kept);
        if (ae) { json_free(plan); json_free(pc); return ae; }
        json_free(pc);
        pc = npc;
    }
    if (start < end) {
        json_t *ow = cbg_offload_window_results(io, st, plan, start, end);
        if (cbg_is_raise(ow)) { json_free(plan); json_free(pc); return ow; }
        json_free(plan);
        plan = json_copy(ow->c.items[0]);
        json_t *offloaded = json_copy(ow->c.items[1]);
        json_t *failed = json_copy(ow->c.items[3]);
        json_free(ow);
        if (cbg_truth(failed)) {
            json_t *la = cbg_args();
            cbg_arg_s(la, "fmt", CBG_ARCHIVE_WARN_FMT);
            json_t *args_arr = json_array();
            json_append(args_arr, json_copy(failed));
            json_set(la, "args", args_arr);
            json_t *lr = cbg_call(io, "logger.warning", la);
            if (cbg_is_raise(lr)) { json_free(plan); json_free(pc); json_free(offloaded); json_free(failed); return lr; }
            json_free(lr);
        }
        json_t *ae = NULL;
        json_t *npc = cbg_arith(pc, offloaded, '+', &ae);
        json_free(offloaded); json_free(failed);
        if (ae) { json_free(plan); json_free(pc); return ae; }
        json_free(pc);
        pc = npc;
    }
    /* getattr(self, "tail_mode", "lean") == "lean" */
    const json_t *tm = cbg_get(st, "tail_mode");
    int lean = tm ? cbg_str_eq(tm, "lean") : 1;
    if (lean) {
        json_t *a = cbg_args();
        cbg_arg_j(a, "messages", plan);
        cbg_arg_n(a, "tail_start", end);
        json_t *dr = cbg_call(io, "_demote_stale_tail_tools", a);
        if (cbg_is_raise(dr)) { json_free(plan); json_free(pc); return dr; }
        json_free(plan);
        plan = dr;
    }
    json_t *out = json_array();
    json_append(out, plan);
    json_append(out, pc);
    return out;
}

/* PoP: _assemble_with_archive_list @ agent/context_compressor_window.py:_assemble_with_archive_list */
static json_t *cbg_assemble_with_archive_list(cbg_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *messages = cbg_get(in, "messages");
    double start = cbg_num(cbg_get(in, "start"));
    double end = cbg_num(cbg_get(in, "end"));
    const json_t *scan = cbg_get(in, "scan");
    const json_t *summary = cbg_get(in, "summary");
    json_t *a = cbg_args();
    cbg_arg_j(a, "rows", messages);
    json_t *named_r = cbg_call(io, "pruned_archive_refs", a);
    if (cbg_is_raise(named_r)) return named_r;
    /* scan.tail_start (attribute double read) */
    const json_t *tsv = cbg_getattr(scan, "tail_start");
    if (!tsv) {
        json_free(named_r);
        return cbg_attr_raise(scan, "tail_start");
    }
    double tail_start = cbg_num(tsv);
    /* kept = [*messages[:start], *messages[max(end, scan.tail_start):],
     *         {"content": summary}] */
    size_t n = cbg_is_arr(messages) ? messages->c.count : 0;
    json_t *kept = json_array();
    size_t a1, b1;
    cbg_slice_bounds((double)n, 0, start, &a1, &b1);
    for (size_t i = a1; i < b1; i++) json_append(kept, json_copy(messages->c.items[i]));
    double cut = end > tail_start ? end : tail_start;
    cbg_slice_bounds((double)n, cut, (double)n, &a1, &b1);
    for (size_t i = a1; i < b1; i++) json_append(kept, json_copy(messages->c.items[i]));
    json_t *srow = json_object();
    json_set(srow, "content", summary ? json_copy(summary) : json_null());
    json_append(kept, srow);
    json_t *err = NULL;
    json_t *named = cbg_sorted_unique(named_r, &err);
    json_free(named_r);
    if (err) { json_free(kept); return err; }
    json_t *a2 = cbg_args();
    cbg_arg_j(a2, "named", named);
    cbg_arg_j(a2, "kept", kept);
    json_free(named); json_free(kept);
    json_t *al = cbg_call(io, "archive_list_for_summary", a2);
    if (cbg_is_raise(al)) return al;
    /* summary + archive_list_for_summary(...) */
    if (!cbg_is_str(summary)) {
        json_free(al);
        char *m = cbg_xfmt("unsupported operand type(s) for +: '%s' and 'str'",
                           cbg_typename(summary));
        json_t *r = cbg_mkraise("TypeError", m ? m : "");
        free(m);
        return r;
    }
    if (!cbg_is_str(al)) {
        char *m = cbg_xfmt("can only concatenate str (not \"%s\") to str", cbg_typename(al));
        json_t *r = cbg_mkraise("TypeError", m ? m : "");
        free(m);
        json_free(al);
        return r;
    }
    char *concat = cbg_xfmt("%s%s", summary->str_val ? summary->str_val : "",
                            al->str_val ? al->str_val : "");
    json_free(al);
    json_t *a3 = cbg_args();
    cbg_arg_j(a3, "messages", messages);
    cbg_arg_n(a3, "start", start);
    cbg_arg_n(a3, "end", end);
    cbg_arg_j(a3, "scan", scan);
    cbg_arg_s(a3, "summary", concat ? concat : "");
    free(concat);
    return cbg_call(io, "_assemble_compressed", a3);
}

/* PoP: _take_prepared_summary @ agent/context_compressor_window.py:_take_prepared_summary */
static json_t *cbg_take_prepared_summary(cbg_io_t *io, const json_t *in, json_t *st)
{
    const json_t *messages = cbg_get(in, "messages");
    double compress_start = cbg_num(cbg_get(in, "compress_start"));
    const json_t *fresh = cbg_get(in, "fresh");
    const json_t *provider_context = cbg_get(in, "provider_context");
    /* self.prepared_compaction (class default None when absent) */
    const json_t *pc = cbg_get(st, "prepared_compaction");
    if (cbg_is_null(pc)) return NULL;
    if (cbg_truth(provider_context)) {
        if (!cbg_is_obj(pc)) return cbg_attr_raise(pc, "stop_for_provider_context");
        json_t *sr = cbg_call(io, "stop_for_provider_context", cbg_args());
        if (cbg_is_raise(sr)) return sr;
        json_free(sr);
    }
    /* self.prepared_compaction.take carrier check (outer attr before args) */
    if (!cbg_is_obj(pc)) return cbg_attr_raise(pc, "take");
    /* self._session_id: DIRECT read, before the take seam fires */
    const json_t *sid = cbg_get(st, "_session_id");
    if (!sid) return cbg_self_raise("_session_id");
    int eligible = !(cbg_truth(fresh) || cbg_truth(provider_context));
    json_t *a = cbg_args();
    cbg_arg_j(a, "messages", messages);
    cbg_arg_j(a, "session_id", sid);
    cbg_arg_n(a, "compress_start", compress_start);
    cbg_arg_b(a, "eligible", eligible);
    return cbg_call(io, "take", a);
}

/* PoP: _adopt_prepared_summary @ agent/context_compressor_window.py:_adopt_prepared_summary */
static json_t *cbg_adopt_prepared_summary(cbg_io_t *io, const json_t *in, json_t *st)
{
    const json_t *prepared = cbg_get(in, "prepared");
    if (!cbg_is_plain_dict(prepared))
        return cbg_type_raise("TypeError", "'%s' object is not a mapping", prepared);
    /* from agent.prepared_compaction import adopt (silent bind); adopt(self,
     * prepared) — compressor projects to the instance-attribute state dict */
    json_t *a = cbg_args();
    cbg_arg_j(a, "compressor", st);
    cbg_arg_j(a, "prepared", prepared);
    json_t *ar = cbg_call(io, "adopt", a);
    if (cbg_is_raise(ar)) return ar;
    json_free(ar);
    /* prepared.summary (SimpleNamespace double in the oracle) */
    const json_t *summary = json_obj_get(prepared, "summary");
    if (!summary)
        return cbg_mkraise("AttributeError",
                           "'types.SimpleNamespace' object has no attribute 'summary'");
    return json_copy(summary);
}