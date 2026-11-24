/*
 * _portability.c -- C11 port of the logical session export/import contract of
 * upstream Hermes persistence/sqlite/housekeeping/_portability.py
 * (gate w33_portabilit_b54bb9).
 *
 * PoP: w33_portabilit_b54bb9_constants @ persistence/sqlite/housekeeping/_portability.py:_IMPORT_SESSION_INSERT_SQL + _IMPORT_*_COLS + _EXPORT_COLUMNS + persistence/housekeeping/maintenance.py:IMPORT_MAX_*
 * PoP: w33_portabilit_b54bb9_export_row @ persistence/sqlite/housekeeping/_portability.py:_export_row
 * PoP: w33_portabilit_b54bb9_text_or_none @ persistence/sqlite/housekeeping/_portability.py:_import_text_or_none
 * PoP: w33_portabilit_b54bb9_int_or_none @ persistence/sqlite/housekeeping/_portability.py:_import_int_or_none
 * PoP: w33_portabilit_b54bb9_int_or_bounded @ persistence/sqlite/housekeeping/_portability.py:_import_int_or_bounded (+ _coerce_or)
 * PoP: w33_portabilit_b54bb9_float_or_finite @ persistence/sqlite/housekeeping/_portability.py:_import_float_or_finite
 * PoP: w33_portabilit_b54bb9_json_object_or_none @ persistence/sqlite/housekeeping/_portability.py:_import_json_object_or_none
 * PoP: w33_portabilit_b54bb9_non_finite_path @ persistence/sqlite/housekeeping/_portability.py:_import_non_finite_path
 * PoP: w33_portabilit_b54bb9_load_json @ persistence/sqlite/housekeeping/_portability.py:_import_load_json
 * PoP: w33_portabilit_b54bb9_message_content @ persistence/sqlite/housekeeping/_portability.py:_validate_import_message_content
 * PoP: w33_portabilit_b54bb9_normalize_message @ persistence/sqlite/housekeeping/_portability.py:_normalize_import_message
 * PoP: w33_portabilit_b54bb9_normalize_session @ persistence/sqlite/housekeeping/_portability.py:_normalize_import_session
 * PoP: w33_portabilit_b54bb9_validate_payload @ persistence/sqlite/housekeeping/_portability.py:_validate_import_payload + _validate_import_session
 * PoP: w33_portabilit_b54bb9_foreign_match @ persistence/sqlite/housekeeping/_portability.py:_find_foreign_import_on_conn
 * PoP: w33_portabilit_b54bb9_parent_attach @ persistence/sqlite/housekeeping/_portability.py:_attach_import_parents
 * PoP: w33_portabilit_b54bb9_import_flags @ persistence/sqlite/housekeeping/_portability.py:_import_session_row (flag mask)
 * PoP: w33_portabilit_b54bb9_message_split @ persistence/sqlite/housekeeping/_portability.py:_import_session_row (live/archived split)
 *
 * What is ported is exactly what the values decide: which payload entries are
 * refused and with which message, how a stored row shapes into the export
 * record, which imported row is the same foreign snapshot, which parent edge
 * closes a cycle, and the flag/visibility mask the row writer derives.
 *
 * Seams left to the caller: the writer/reader connection (the SELECT/INSERT/
 * UPDATE text and row order), holder._store_system_prompt, _insert_message_rows
 * and the checkpoint prune, the clock (time.time), the minted session id, and
 * the decoder's own JSONDecodeError text -- libjson is a strict-JSON decoder
 * while CPython's json.loads also accepts the NaN/Infinity literals, so that
 * rejection is compared by decision (kind), not by message.  libjson holds one
 * number type: an integral double is rendered as an integer, so payload
 * numbers are taken as integers (CPython would keep 1.0 apart from 1) and
 * int() saturation at the 64-bit edge is invisible because the SQLite range
 * check in front of every write rejects anything wider.
 */
#include "hermes_json.h"
#include "noc_w33_portabilit_b54bb9.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- budget constants (persistence/housekeeping/maintenance.py) -------- */
#define PB_MAX_SESSIONS               500.0
#define PB_MAX_MESSAGES_PER_SESSION   10000.0
#define PB_MAX_TOTAL_MESSAGES         50000.0
#define PB_MAX_SESSION_BYTES          5242880.0
#define PB_MAX_TOTAL_BYTES            26214400.0
/* conversation/_store/portability.py:_SQLITE_INT_{MIN,MAX} */
#define PB_SQLITE_INT_MIN_D          (-9223372036854775808.0)
#define PB_SQLITE_INT_MAX_D          9223372036854775808.0   /* 2**63 */
#define PB_SQLITE_INT_MIN_TEXT       "-9223372036854775808"
#define PB_SQLITE_INT_MAX_TEXT       "9223372036854775807"
/* conversation/_store/rules.py:EPOCH_{MIN,MAX} (coerce_epoch) */
#define PB_EPOCH_MIN                 0.0
#define PB_EPOCH_MAX                 4200000000.0

/* ---- growable buffer --------------------------------------------------- */
typedef struct { char *p; size_t len, cap; int oom; } pb_buf;

static void pb_reserve(pb_buf *b, size_t extra) {
    size_t need, cap;
    char *np;
    if (b->oom) return;
    need = b->len + extra + 1;
    if (need <= b->cap) return;
    cap = b->cap ? b->cap : 64;
    while (cap < need) cap <<= 1;
    np = (char *)realloc(b->p, cap);
    if (!np) { b->oom = 1; return; }
    b->p = np;
    b->cap = cap;
}

static void pb_putn(pb_buf *b, const char *s, size_t n) {
    if (b->oom) return;
    pb_reserve(b, n);
    if (b->oom) return;
    if (n) memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void pb_puts(pb_buf *b, const char *s) { if (s) pb_putn(b, s, strlen(s)); }
static void pb_putc(pb_buf *b, char c) { pb_putn(b, &c, 1); }

static void pb_putf(pb_buf *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof(tmp)) { pb_putn(b, tmp, (size_t)n); return; }
    {
        char *big = (char *)malloc((size_t)n + 1);
        if (!big) { b->oom = 1; return; }
        va_start(ap, fmt);
        vsnprintf(big, (size_t)n + 1, fmt, ap);
        va_end(ap);
        pb_putn(b, big, (size_t)n);
        free(big);
    }
}

static void pb_init(pb_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; b->oom = 0; }
static void pb_free(pb_buf *b) { free(b->p); b->p = NULL; b->len = 0; b->cap = 0; b->oom = 0; }
static const char *pb_cstr(pb_buf *b) { return b->p ? b->p : ""; }

/* ---- Python value primitives ------------------------------------------ */

static int pb_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;   /* nan is truthy */
    case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
    case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
    case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
    }
    return 0;
}

static int pb_numberish(const json_t *v) {
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}

static double pb_as_double(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return v->num_val;
}

/* Python '==' over JSON shapes: numbers and bools compare numerically. */
static int pb_equal(const json_t *a, const json_t *b) {
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (pb_numberish(a) && pb_numberish(b)) return pb_as_double(a) == pb_as_double(b);
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JSON_STRING:
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY:
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!pb_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    case JSON_OBJECT:
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !pb_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    default:
        return 0;
    }
}

/* Python str.strip() over the ASCII whitespace set. */
static void pb_strip(pb_buf *b) {
    size_t start = 0, end = b->len;
    if (!b->p) return;
    while (start < end) {
        char c = b->p[start];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') start++;
        else break;
    }
    while (end > start) {
        char c = b->p[end - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') end--;
        else break;
    }
    if (start) memmove(b->p, b->p + start, end - start);
    b->len = end - start;
    b->p[b->len] = '\0';
}

/* Python repr()/str() number text: shortest round-trip form. */
static void pb_num_text(char *out, size_t outsz, double v) {
    int prec;
    if (!isfinite(v)) {
        snprintf(out, outsz, "%s", (v != v) ? "NaN" : (v > 0 ? "Infinity" : "-Infinity"));
        return;
    }
    if (v == floor(v) && fabs(v) < PB_SQLITE_INT_MAX_D) {
        snprintf(out, outsz, "%lld", (long long)v);
        return;
    }
    for (prec = 1; prec <= 17; prec++) {
        snprintf(out, outsz, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
}

/* ---- json.dumps (ensure_ascii / separators as arguments) --------------- */

static void pb_json_str(pb_buf *b, const char *s, int ascii) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    pb_putc(b, '"');
    while (*p) {
        unsigned char c = *p;
        if (c == '"') { pb_puts(b, "\\\""); p++; continue; }
        if (c == '\\') { pb_puts(b, "\\\\"); p++; continue; }
        if (c == '\b') { pb_puts(b, "\\b"); p++; continue; }
        if (c == '\f') { pb_puts(b, "\\f"); p++; continue; }
        if (c == '\n') { pb_puts(b, "\\n"); p++; continue; }
        if (c == '\r') { pb_puts(b, "\\r"); p++; continue; }
        if (c == '\t') { pb_puts(b, "\\t"); p++; continue; }
        if (c < 0x20) { pb_putf(b, "\\u%04x", (unsigned)c); p++; continue; }
        if (c < 0x80 || !ascii) { pb_putc(b, (char)c); p++; continue; }
        {
            unsigned cp;
            int n, i;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; n = 1; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; n = 2; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; n = 3; }
            else { pb_putc(b, (char)c); p++; continue; }
            p++;
            for (i = 0; i < n; i++) {
                if ((*p & 0xC0) != 0x80) break;
                cp = (cp << 6) | (*p & 0x3Fu);
                p++;
            }
            if (cp >= 0x10000u) {
                unsigned v = cp - 0x10000u;
                pb_putf(b, "\\u%04x\\u%04x", 0xD800u + (v >> 10), 0xDC00u + (v & 0x3FFu));
            } else {
                pb_putf(b, "\\u%04x", cp);
            }
        }
    }
    pb_putc(b, '"');
}

static void pb_dumps(pb_buf *b, const json_t *v, int ascii, const char *sep_comma,
                     const char *sep_colon) {
    size_t i;
    if (!v || v->type == JSON_NULL) { pb_puts(b, "null"); return; }
    switch (v->type) {
    case JSON_BOOL:
        pb_puts(b, v->bool_val ? "true" : "false");
        return;
    case JSON_NUMBER: {
        char tmp[64];
        pb_num_text(tmp, sizeof(tmp), v->num_val);
        pb_puts(b, tmp);
        return;
    }
    case JSON_STRING:
        pb_json_str(b, v->str_val, ascii);
        return;
    case JSON_ARRAY:
        pb_putc(b, '[');
        for (i = 0; i < json_len(v); i++) {
            if (i) pb_puts(b, sep_comma);
            pb_dumps(b, json_get(v, i), ascii, sep_comma, sep_colon);
        }
        pb_putc(b, ']');
        return;
    case JSON_OBJECT:
        pb_putc(b, '{');
        for (i = 0; i < json_object_size(v); i++) {
            const char *k = json_object_get_key_at(v, i);
            if (i) pb_puts(b, sep_comma);
            pb_json_str(b, k ? k : "", ascii);
            pb_puts(b, sep_colon);
            pb_dumps(b, json_object_get_at(v, i), ascii, sep_comma, sep_colon);
        }
        pb_putc(b, '}');
        return;
    default:
        pb_puts(b, "null");
        return;
    }
}

/* Python repr() of a value (containers included), for str(id) of an object. */
static void pb_py_repr(pb_buf *b, const json_t *v) {
    size_t i;
    if (!v) { pb_puts(b, "None"); return; }
    switch (v->type) {
    case JSON_NULL: pb_puts(b, "None"); return;
    case JSON_BOOL: pb_puts(b, v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER: {
        char tmp[64];
        pb_num_text(tmp, sizeof(tmp), v->num_val);
        pb_puts(b, tmp);
        return;
    }
    case JSON_STRING: {
        const char *s = v->str_val ? v->str_val : "";
        pb_putc(b, '\'');
        for (; *s; s++) {
            if (*s == '\'') pb_puts(b, "\\'");
            else if (*s == '\\') pb_puts(b, "\\\\");
            else if (*s == '\n') pb_puts(b, "\\n");
            else if (*s == '\r') pb_puts(b, "\\r");
            else if (*s == '\t') pb_puts(b, "\\t");
            else pb_putc(b, *s);
        }
        pb_putc(b, '\'');
        return;
    }
    case JSON_ARRAY:
        pb_putc(b, '[');
        for (i = 0; i < json_len(v); i++) {
            if (i) pb_puts(b, ", ");
            pb_py_repr(b, json_get(v, i));
        }
        pb_putc(b, ']');
        return;
    case JSON_OBJECT:
        pb_putc(b, '{');
        for (i = 0; i < json_object_size(v); i++) {
            const char *k = json_object_get_key_at(v, i);
            if (i) pb_puts(b, ", ");
            pb_json_str(b, k ? k : "", 0);
            pb_puts(b, ": ");
            pb_py_repr(b, json_object_get_at(v, i));
        }
        pb_putc(b, '}');
        return;
    default:
        pb_puts(b, "None");
        return;
    }
}

/* str(value) for the shapes a payload id can hold. */
static void pb_py_str(pb_buf *b, const json_t *v) {
    if (v && v->type == JSON_STRING) { pb_puts(b, v->str_val ? v->str_val : ""); return; }
    pb_py_repr(b, v);
}

/* ---- failure envelope -------------------------------------------------- */

typedef struct { const char *kind; pb_buf msg; } pb_fail;

static void pb_failf(pb_fail *f, const char *kind, const char *fmt, ...) {
    char tmp[2048];
    va_list ap;
    f->kind = kind;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    pb_free(&f->msg);
    pb_puts(&f->msg, tmp);
}

static json_t *pb_ok(json_t *value) {
    json_t *o = json_object();
    if (!o) { json_free(value); return NULL; }
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", value ? value : json_null());
    return o;
}

static json_t *pb_err_env(const char *kind, const char *msg) {
    json_t *o = json_object();
    if (!o) return NULL;
    json_set(o, "ok", json_bool(false));
    json_set(o, "kind", json_string(kind ? kind : "Error"));
    json_set(o, "error", json_string(msg ? msg : ""));
    return o;
}

static json_t *pb_finish(json_t *value, pb_fail *f) {
    json_t *env;
    if (value) env = pb_ok(value);
    else env = pb_err_env(f->kind ? f->kind : "ValueError", pb_cstr(&f->msg));
    pb_free(&f->msg);
    return env;
}

/* ---- column families --------------------------------------------------- */

/* _IMPORT_SESSION_TEXT_FIELDS (``role`` is validated separately). */
#define PB_SESSION_TEXT_N 15
static const char *const PB_SESSION_TEXT[PB_SESSION_TEXT_N] = {
    "source", "user_id", "model", "system_prompt", "end_reason", "cwd", "git_branch",
    "git_repo_root", "billing_provider", "billing_base_url", "billing_mode", "cost_status",
    "cost_source", "pricing_version", "title",
};

/* _IMPORT_MESSAGE_TEXT_FIELDS */
#define PB_MESSAGE_TEXT_N 8
static const char *const PB_MESSAGE_TEXT[PB_MESSAGE_TEXT_N] = {
    "tool_call_id", "tool_name", "effect_disposition", "finish_reason", "reasoning",
    "reasoning_content", "platform_message_id", "message_id",
};

/* _IMPORT_MESSAGE_JSON_FIELDS */
#define PB_MESSAGE_JSON_N 5
static const char *const PB_MESSAGE_JSON[PB_MESSAGE_JSON_N] = {
    "reasoning_details", "codex_reasoning_items", "codex_message_items",
    "absorbed_message_uids", "tool_call_uids",
};

/* _IMPORT_PASSTHROUGH_COLS */
#define PB_PASSTHROUGH_N 14
static const char *const PB_PASSTHROUGH[PB_PASSTHROUGH_N] = {
    "user_id", "model", "model_config", "end_reason", "cwd", "git_branch", "git_repo_root",
    "billing_provider", "billing_base_url", "billing_mode", "cost_status", "cost_source",
    "pricing_version", "title",
};

/* _IMPORT_INT_COLS */
#define PB_INT_COLS_N 6
static const char *const PB_INT_COLS[PB_INT_COLS_N] = {
    "input_tokens", "output_tokens", "cache_read_tokens", "cache_write_tokens",
    "reasoning_tokens", "api_call_count",
};

/* _IMPORT_FLOAT_COLS */
#define PB_FLOAT_COLS_N 3
static const char *const PB_FLOAT_COLS[PB_FLOAT_COLS_N] = {
    "ended_at", "estimated_cost_usd", "actual_cost_usd",
};

/* _IMPORT_FLAG_COLS: pinned/hidden/auto_archived drive the prune, listings and
 * resume-unarchive, so a restore must keep them. */
#define PB_FLAG_COLS_N 4
static const char *const PB_FLAG_COLS[PB_FLAG_COLS_N] = {
    "archived", "auto_archived", "pinned", "hidden",
};

/* _EXPORT_COLUMNS: every SessionExportRow field but last_active, reset_from and
 * branched_from (those three are looked up with .get). */
#define PB_EXPORT_COLS_N 66
static const char *const PB_EXPORT_COLS[PB_EXPORT_COLS_N] = {
    "id", "runtime_revision", "runtime_generation", "source", "created_source", "user_id",
    "session_key", "chat_id", "chat_type", "thread_id", "display_name", "origin_json",
    "expiry_finalized", "model", "model_config", "system_prompt", "system_prompt_hash",
    "parent_session_id", "started_at", "ended_at", "end_reason", "message_count",
    "tool_call_count", "input_tokens", "output_tokens", "cache_read_tokens",
    "cache_write_tokens", "reasoning_tokens", "cwd", "git_branch", "git_repo_root",
    "git_metadata_generation", "billing_provider", "billing_base_url", "billing_mode",
    "estimated_cost_usd", "actual_cost_usd", "cost_status", "cost_source", "pricing_version",
    "title", "title_source", "last_activity_at", "last_activity_description",
    "last_activity_provenance", "api_call_count", "handoff_state", "handoff_platform",
    "handoff_error", "compression_failure_cooldown_until", "compression_failure_error",
    "compression_fallback_streak", "compression_ineffective_count",
    "compression_recovery_deadline", "compression_overload_streak", "profile_name",
    "transport_profile", "rewind_count", "archived", "auto_archived", "pinned", "hidden",
    "last_read_at", "tool_names", "transcript_revision", "prompt_cache_clock",
};

/* _IMPORT_SESSION_INSERT_SQL */
static const char *const PB_INSERT_SQL =
    "INSERT INTO sessions (\n"
    "                           id, source, user_id, model, model_config, system_prompt,\n"
    "                           system_prompt_hash,\n"
    "                           parent_session_id, started_at, ended_at, end_reason,\n"
    "                           message_count, tool_call_count, input_tokens, output_tokens,\n"
    "                           cache_read_tokens, cache_write_tokens, reasoning_tokens,\n"
    "                           cwd, git_branch, git_repo_root,\n"
    "                           billing_provider, billing_base_url, billing_mode,\n"
    "                           estimated_cost_usd, actual_cost_usd, cost_status, cost_source,\n"
    "                           pricing_version, title, api_call_count, archived, auto_archived, pinned, hidden\n"
    "                       )\n"
    "                       VALUES (\n"
    "                           :id, :source, :user_id, :model, :model_config,\n"
    "                           NULL, :system_prompt_hash, NULL, :started_at, :ended_at,\n"
    "                           :end_reason, 0, 0, :input_tokens, :output_tokens,\n"
    "                           :cache_read_tokens, :cache_write_tokens,\n"
    "                           :reasoning_tokens, :cwd, :git_branch, :git_repo_root,\n"
    "                           :billing_provider, :billing_base_url, :billing_mode,\n"
    "                           :estimated_cost_usd, :actual_cost_usd, :cost_status,\n"
    "                           :cost_source, :pricing_version, :title,\n"
    "                           :api_call_count, :archived, :auto_archived, :pinned, :hidden\n"
    "                       )";

/* ======================================================================= */
/* cores                                                                   */
/* ======================================================================= */

static int pb_int64_ok(double v) {
    return v >= PB_SQLITE_INT_MIN_D && v < PB_SQLITE_INT_MAX_D;
}

static double pb_trunc(double v) { return v < 0 ? ceil(v) : floor(v); }

static int pb_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python int(text): optional sign, digits with single inner underscores,
 * surrounding whitespace.  1 on success, 0 on a parse failure, 2 when the value
 * is wider than 64 bits (the caller saturates: every write path range-checks). */
static int pb_int_text(const char *s, double *out) {
    const char *p = s ? s : "";
    int neg = 0, digits = 0, prev_us = 0, overflow = 0;
    unsigned long long acc = 0;
    while (pb_space(*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            unsigned d = (unsigned)(*p - '0');
            if (!overflow) {
                if (acc > (9223372036854775807ULL - d) / 10ULL) overflow = 1;
                else acc = acc * 10ULL + d;
            }
            digits++;
            prev_us = 0;
            continue;
        }
        if (*p == '_' && digits && !prev_us) { prev_us = 1; continue; }
        break;
    }
    if (prev_us || !digits) return 0;
    while (pb_space(*p)) p++;
    if (*p) return 0;
    if (overflow) { *out = neg ? -1e300 : 1e300; return 2; }
    *out = (double)acc * (neg ? -1.0 : 1.0);
    return 1;
}

/* Python float(text); 0 when it would raise. */
static int pb_float_text(const char *s, double *out) {
    const char *p = s ? s : "";
    const char *q;
    char *end = NULL;
    double d;
    while (pb_space(*p)) p++;
    if (!*p) return 0;
    q = (*p == '+' || *p == '-') ? p + 1 : p;
    if (q[0] == '0' && (q[1] == 'x' || q[1] == 'X')) return 0;   /* strtod's hex floats */
    d = strtod(p, &end);
    if (end == p) return 0;
    while (pb_space(*end)) end++;
    if (*end) return 0;
    *out = d;
    return 1;
}

/* coerce_epoch: a usable epoch float, else 0 (None / "" / untrusted). */
static int pb_coerce_epoch(const json_t *v, double *out) {
    double d;
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_STRING) {
        if (!v->str_val || !v->str_val[0]) return 0;
        if (!pb_float_text(v->str_val, &d)) d = NAN;
    } else if (v->type == JSON_NUMBER) {
        d = v->num_val;
    } else if (v->type == JSON_BOOL) {
        d = v->bool_val ? 1.0 : 0.0;
    } else {
        d = NAN;
    }
    if (!(d >= PB_EPOCH_MIN && d <= PB_EPOCH_MAX)) return 0;
    *out = d;
    return 1;
}

static json_t *pb_core_text_or_none(const json_t *v, const char *field, pb_fail *f) {
    if (!v || v->type == JSON_NULL) return json_null();
    if (v->type == JSON_STRING) return json_copy(v);
    pb_failf(f, "ValueError", "%s must be a string", field);
    return NULL;
}

static json_t *pb_core_int_or_none(const json_t *v, const char *field, pb_fail *f) {
    if (!v || v->type == JSON_NULL) return json_null();
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d != d) {
            pb_failf(f, "ValueError", "%s must be an integer", field);
            return NULL;
        }
        if (isinf(d)) {
            pb_failf(f, "OverflowError", "cannot convert float infinity to integer");
            return NULL;
        }
        d = pb_trunc(d);
        if (!pb_int64_ok(d)) d = (d > 0 ? 1e300 : -1e300);
        return json_number(d);
    }
    if (v->type == JSON_STRING) {
        double d = 0.0;
        if (pb_int_text(v->str_val, &d) == 0) {
            pb_failf(f, "ValueError", "%s must be an integer", field);
            return NULL;
        }
        return json_number(d);
    }
    pb_failf(f, "ValueError", "%s must be an integer", field);
    return NULL;
}

/* _coerce_or(value, int, default): 1 ok (out), 0 -> default, -1 -> error. */
static int pb_coerce_int(const json_t *v, double *out, pb_fail *f) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d != d) return 0;
        if (isinf(d)) {
            pb_failf(f, "OverflowError", "cannot convert float infinity to integer");
            return -1;
        }
        *out = pb_trunc(d);
        return 1;
    }
    if (v->type == JSON_STRING) {
        double d = 0.0;
        if (pb_int_text(v->str_val, &d) == 0) return 0;
        *out = d;
        return 1;
    }
    return 0;
}

static json_t *pb_core_int_or_bounded(const json_t *v, const char *field,
                                      const json_t *fallback, pb_fail *f) {
    double d = 0.0;
    int r;
    (void)field;
    r = pb_coerce_int(v, &d, f);
    if (r < 0) return NULL;
    if (r == 0) return json_copy(fallback);
    if (!pb_int64_ok(d)) return json_copy(fallback);
    return json_number(d);
}

static json_t *pb_core_float_or_finite(const json_t *v, const char *field, pb_fail *f) {
    double d;
    (void)field;
    if (!v || v->type == JSON_NULL) return json_null();
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v->type == JSON_NUMBER) {
        if (!isfinite(v->num_val)) return json_null();
        return json_number(v->num_val);
    }
    if (v->type == JSON_STRING) {
        if (!pb_float_text(v->str_val, &d)) return json_null();
        if (!isfinite(d)) return json_null();
        return json_number(d);
    }
    (void)f;
    return json_null();
}

/* ---- non-finite walk ---------------------------------------------------- */

typedef struct {
    const json_t **vals;
    char **paths;
    size_t n, cap;
} pb_stack;

static int pb_push(pb_stack *st, const json_t *v, const char *path) {
    if (st->n == st->cap) {
        size_t cap = st->cap ? st->cap * 2 : 32;
        const json_t **nv = (const json_t **)realloc((void *)st->vals, cap * sizeof(*nv));
        char **np;
        if (!nv) return 0;
        st->vals = nv;
        np = (char **)realloc(st->paths, cap * sizeof(*np));
        if (!np) return 0;
        st->paths = np;
        st->cap = cap;
    }
    st->vals[st->n] = v;
    {
        size_t ln = strlen(path ? path : "");
        char *copy = (char *)malloc(ln + 1);
        if (!copy) return 0;
        memcpy(copy, path ? path : "", ln + 1);
        st->paths[st->n] = copy;
    }
    st->n++;
    return 1;
}

/* _import_non_finite_path: json_null() when clean, the first path string when a
 * NaN/inf number is found, NULL on allocation failure. */
static json_t *pb_core_non_finite_path(const json_t *value, const char *path, pb_fail *f) {
    pb_stack st;
    json_t *result = NULL;
    int oom = 0;
    st.vals = NULL;
    st.paths = NULL;
    st.n = 0;
    st.cap = 0;
    if (!pb_push(&st, value, path)) oom = 1;
    while (!oom && st.n > 0) {
        const json_t *cur;
        char *cur_path;
        size_t i;
        st.n--;
        cur = st.vals[st.n];
        cur_path = st.paths[st.n];
        if (cur && cur->type == JSON_NUMBER && !isfinite(cur->num_val)) {
            result = json_string(cur_path);
            if (!result) oom = 1;
            free(cur_path);
            break;
        }
        if (cur && cur->type == JSON_OBJECT) {
            for (i = 0; i < json_object_size(cur); i++) {
                const char *k = json_object_get_key_at(cur, i);
                pb_buf np;
                pb_init(&np);
                pb_puts(&np, cur_path);
                pb_putc(&np, '.');
                pb_puts(&np, k ? k : "");
                if (!pb_push(&st, json_object_get_at(cur, i), pb_cstr(&np))) oom = 1;
                pb_free(&np);
                if (oom) break;
            }
        } else if (cur && cur->type == JSON_ARRAY) {
            for (i = 0; i < json_len(cur); i++) {
                pb_buf np;
                pb_init(&np);
                pb_puts(&np, cur_path);
                pb_putf(&np, "[%zu]", i);
                if (!pb_push(&st, json_get(cur, i), pb_cstr(&np))) oom = 1;
                pb_free(&np);
                if (oom) break;
            }
        }
        free(cur_path);
    }
    while (st.n > 0) { st.n--; free(st.paths[st.n]); }
    free((void *)st.vals);
    free(st.paths);
    if (oom) {
        json_free(result);
        pb_failf(f, "MemoryError", "");
        return NULL;
    }
    return result ? result : json_null();
}

/* ---- load_json ---------------------------------------------------------- */

static json_t *pb_core_load_json(const char *text, const char *path, pb_fail *f) {
    json_t *parsed = json_parse(text ? text : "", NULL);
    json_t *found;
    if (!parsed) {
        pb_failf(f, "JSONDecodeError", "");
        return NULL;
    }
    found = pb_core_non_finite_path(parsed, path, f);
    if (!found) { json_free(parsed); return NULL; }
    if (found->type == JSON_STRING) {
        pb_failf(f, "ValueError", "non-finite number at %s", found->str_val);
        json_free(found);
        json_free(parsed);
        return NULL;
    }
    json_free(found);
    return parsed;
}

/* ---- json_object_or_none ------------------------------------------------ */

static json_t *pb_core_json_object_or_none(const json_t *v, const char *field, pb_fail *f) {
    pb_buf path;
    pb_init(&path);
    pb_puts(&path, "session.");
    pb_puts(&path, field);
    if (!v || v->type == JSON_NULL) { pb_free(&path); return json_null(); }
    if (v->type == JSON_STRING) {
        json_t *parsed = json_parse(v->str_val ? v->str_val : "", NULL);
        if (!parsed) {
            pb_failf(f, "ValueError", "%s must be valid JSON", field);
            pb_free(&path);
            return NULL;
        }
        if (parsed->type != JSON_OBJECT) {
            pb_failf(f, "ValueError", "%s must be a JSON object", field);
            json_free(parsed);
            pb_free(&path);
            return NULL;
        }
        {
            json_t *found = pb_core_non_finite_path(parsed, pb_cstr(&path), f);
            if (!found) { json_free(parsed); pb_free(&path); return NULL; }
            if (found->type == JSON_STRING) {
                pb_failf(f, "ValueError", "non-finite number at %s", found->str_val);
                json_free(found);
                json_free(parsed);
                pb_free(&path);
                return NULL;
            }
            json_free(found);
        }
        json_free(parsed);
        pb_free(&path);
        return json_copy(v);        /* the stored column keeps the original text */
    }
    if (v->type != JSON_OBJECT) {
        pb_failf(f, "ValueError", "%s must be a JSON object", field);
        pb_free(&path);
        return NULL;
    }
    {
        json_t *found = pb_core_non_finite_path(v, pb_cstr(&path), f);
        if (!found) { pb_free(&path); return NULL; }
        if (found->type == JSON_STRING) {
            pb_failf(f, "ValueError", "non-finite number at %s", pb_cstr(&path));
            json_free(found);
            pb_free(&path);
            return NULL;
        }
        json_free(found);
    }
    pb_free(&path);
    {
        pb_buf text;
        json_t *s;
        pb_init(&text);
        pb_dumps(&text, v, 1, ", ", ": ");
        if (text.oom) { pb_free(&text); pb_failf(f, "MemoryError", ""); return NULL; }
        s = json_string(pb_cstr(&text));
        pb_free(&text);
        return s;
    }
}

/* ---- message content ---------------------------------------------------- */

static json_t *pb_core_message_content(const json_t *content, const char *path, pb_fail *f) {
    if (content && content->type == JSON_NUMBER) {
        double d = content->num_val;
        if (isfinite(d) && d == pb_trunc(d) && !pb_int64_ok(d)) {
            pb_failf(f, "ValueError", "%s.content is outside SQLite's integer range", path);
            return NULL;
        }
        if (!isfinite(d)) {
            pb_failf(f, "ValueError", "non-finite number at %s.content", path);
            return NULL;
        }
    }
    /* bytes/bytearray cannot arrive through JSON: that branch has no C shape.
     * The successful value is null; the caller reads success from the pointer. */
    return json_null();
}

/* ---- normalize one message ---------------------------------------------- */

static json_t *pb_core_normalize_message(const json_t *msg, int index, pb_fail *f) {
    json_t *clean;
    const json_t *role, *ts;
    char message_path[64];
    size_t i;

    if (!msg || msg->type != JSON_OBJECT) {
        pb_failf(f, "TypeError", "message must be an object");
        return NULL;
    }
    snprintf(message_path, sizeof(message_path), "session.messages[%d]", index);
    clean = json_copy(msg);
    role = json_obj_get(msg, "role");
    if (!role || role->type != JSON_STRING || !role->str_val || !role->str_val[0]) {
        pb_failf(f, "ValueError", "messages[%d].role must be a non-empty string", index);
        json_free(clean);
        return NULL;
    }
    for (i = 0; i < PB_MESSAGE_TEXT_N; i++) {
        json_t *t = pb_core_text_or_none(json_obj_get(msg, PB_MESSAGE_TEXT[i]), PB_MESSAGE_TEXT[i], f);
        if (!t) { json_free(clean); return NULL; }
        json_set(clean, PB_MESSAGE_TEXT[i], t);
    }
    {
        json_t *tok = pb_core_int_or_none(json_obj_get(msg, "token_count"), "token_count", f);
        if (!tok) { json_free(clean); return NULL; }
        if (tok->type == JSON_NUMBER && !pb_int64_ok(tok->num_val)) {
            pb_failf(f, "ValueError", "token_count is outside SQLite's integer range");
            json_free(tok);
            json_free(clean);
            return NULL;
        }
        json_set(clean, "token_count", tok);
    }
    ts = json_obj_get(msg, "timestamp");
    if (ts && ts->type != JSON_NULL) {
        double epoch = 0.0;
        if (!pb_coerce_epoch(ts, &epoch)) {
            pb_failf(f, "ValueError", "%s.timestamp must be a supported Unix timestamp", message_path);
            json_free(clean);
            return NULL;
        }
    }
    {
        json_t *okv = pb_core_message_content(json_obj_get(msg, "content"), message_path, f);
        if (!okv) { json_free(clean); return NULL; }
        json_free(okv);
    }
    for (i = 0; i < PB_MESSAGE_JSON_N; i++) {
        const json_t *jv = json_obj_get(msg, PB_MESSAGE_JSON[i]);
        if (!jv || jv->type == JSON_NULL) continue;
        if (jv->type == JSON_STRING) {
            /* Malformed JSON text persists verbatim; only a parsed non-finite
             * number is refused. */
            json_t *parsed = json_parse(jv->str_val ? jv->str_val : "", NULL);
            if (parsed) {
                pb_buf jp;
                json_t *found;
                pb_init(&jp);
                pb_puts(&jp, message_path);
                pb_putc(&jp, '.');
                pb_puts(&jp, PB_MESSAGE_JSON[i]);
                found = pb_core_non_finite_path(parsed, pb_cstr(&jp), f);
                if (!found) {
                    pb_free(&jp);
                    json_free(parsed);
                    json_free(clean);
                    return NULL;
                }
                if (found->type == JSON_STRING) {
                    pb_failf(f, "ValueError", "non-finite number at %s", found->str_val);
                    json_free(found);
                    pb_free(&jp);
                    json_free(parsed);
                    json_free(clean);
                    return NULL;
                }
                json_free(found);
                pb_free(&jp);
                json_free(parsed);
            }
        } else {
            pb_buf jp;
            json_t *found;
            pb_init(&jp);
            pb_puts(&jp, message_path);
            pb_putc(&jp, '.');
            pb_puts(&jp, PB_MESSAGE_JSON[i]);
            found = pb_core_non_finite_path(jv, pb_cstr(&jp), f);
            if (!found) {
                pb_free(&jp);
                json_free(clean);
                return NULL;
            }
            if (found->type == JSON_STRING) {
                pb_failf(f, "ValueError", "non-finite number at %s", found->str_val);
                json_free(found);
                pb_free(&jp);
                json_free(clean);
                return NULL;
            }
            json_free(found);
            pb_free(&jp);
        }
    }
    return clean;
}

/* ---- normalize one session ---------------------------------------------- */

static json_t *pb_core_normalize_session(const json_t *raw, const char *session_id,
                                         const json_t *messages, pb_fail *f) {
    json_t *clean, *out, *list;
    size_t i;

    if (!raw || raw->type != JSON_OBJECT) {
        pb_failf(f, "TypeError", "session must be an object");
        return NULL;
    }
    clean = json_copy(raw);
    json_set(clean, "id", json_string(session_id ? session_id : ""));
    {
        json_t *mc = pb_core_json_object_or_none(json_obj_get(raw, "model_config"), "model_config", f);
        if (!mc) { json_free(clean); return NULL; }
        json_set(clean, "model_config", mc);
    }
    {
        json_t *t = pb_core_text_or_none(json_obj_get(raw, "parent_session_id"), "parent_session_id", f);
        if (!t) { json_free(clean); return NULL; }
        json_set(clean, "parent_session_id", t);
    }
    for (i = 0; i < PB_SESSION_TEXT_N; i++) {
        json_t *t = pb_core_text_or_none(json_obj_get(raw, PB_SESSION_TEXT[i]), PB_SESSION_TEXT[i], f);
        if (!t) { json_free(clean); return NULL; }
        json_set(clean, PB_SESSION_TEXT[i], t);
    }
    {
        const json_t *started = json_obj_get(raw, "started_at");
        if (started && started->type != JSON_NULL) {
            double epoch = 0.0;
            if (!pb_coerce_epoch(started, &epoch)) {
                pb_failf(f, "ValueError", "session.started_at must be a supported Unix timestamp");
                json_free(clean);
                return NULL;
            }
        }
    }
    for (i = 0; i < PB_FLOAT_COLS_N; i++) {
        const json_t *v = json_obj_get(raw, PB_FLOAT_COLS[i]);
        if (v && v->type == JSON_NUMBER && !isfinite(v->num_val)) {
            pb_failf(f, "ValueError", "non-finite number at session.%s", PB_FLOAT_COLS[i]);
            json_free(clean);
            return NULL;
        }
    }
    for (i = 0; i < PB_INT_COLS_N; i++) {
        const json_t *v = json_obj_get(raw, PB_INT_COLS[i]);
        if (v && v->type == JSON_NUMBER) {
            double d = v->num_val;
            if (isfinite(d) && d == pb_trunc(d) && !pb_int64_ok(d)) {
                pb_failf(f, "ValueError", "%s is outside SQLite's integer range", PB_INT_COLS[i]);
                json_free(clean);
                return NULL;
            }
        }
    }
    list = json_array();
    if (messages && messages->type == JSON_ARRAY) {
        for (i = 0; i < json_len(messages); i++) {
            json_t *item = pb_core_normalize_message(json_get(messages, i), (int)i, f);
            if (!item) { json_free(list); json_free(clean); return NULL; }
            json_append(list, item);
        }
    } else if (messages && messages->type != JSON_NULL) {
        pb_failf(f, "TypeError", "messages must be a list");
        json_free(list);
        json_free(clean);
        return NULL;
    }
    out = json_object();
    json_set(out, "session", clean);
    json_set(out, "messages", list);
    return out;
}

/* ---- payload validation ------------------------------------------------- */

static int pb_seen_has(const json_t *seen, const char *sid) {
    size_t i;
    for (i = 0; i < json_len(seen); i++) {
        const json_t *v = json_get(seen, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val ? v->str_val : "", sid)) return 1;
    }
    return 0;
}

static json_t *pb_core_validate_session(const json_t *raw, const char *sid, json_t *seen,
                                        double *total_bytes, double *total_messages,
                                        pb_fail *f) {
    const json_t *messages;
    json_t *measured, *item;
    pb_buf text;
    double session_bytes;
    size_t i;

    if (!raw || raw->type != JSON_OBJECT) {
        pb_failf(f, "ValueError", "session must be an object");
        return NULL;
    }
    if (!sid || !sid[0]) {
        pb_failf(f, "ValueError", "session id is required");
        return NULL;
    }
    if (pb_seen_has(seen, sid)) {
        pb_failf(f, "ValueError", "duplicate session id");
        return NULL;
    }
    messages = json_obj_get(raw, "messages");
    if (!pb_truthy(messages)) messages = NULL;
    if (messages && messages->type != JSON_ARRAY) {
        pb_failf(f, "ValueError", "messages must be a list");
        return NULL;
    }
    if (messages && (double)json_len(messages) > PB_MAX_MESSAGES_PER_SESSION) {
        pb_failf(f, "ValueError", "messages exceeds the per-session import limit");
        return NULL;
    }
    if (messages) {
        for (i = 0; i < json_len(messages); i++) {
            const json_t *m = json_get(messages, i);
            if (!m || m->type != JSON_OBJECT) {
                pb_failf(f, "ValueError", "messages must contain only objects");
                return NULL;
            }
        }
    }
    /* `timings` is rebuilt on the next export: it must not eat the budget of
     * the content it describes. */
    measured = json_object();
    for (i = 0; i < json_object_size(raw); i++) {
        const char *k = json_object_get_key_at(raw, i);
        if (!k || !strcmp(k, "timings")) continue;
        json_set(measured, k, json_copy(json_object_get_at(raw, i)));
    }
    pb_init(&text);
    pb_dumps(&text, measured, 0, ",", ":");
    json_free(measured);
    if (text.oom) { pb_free(&text); pb_failf(f, "MemoryError", ""); return NULL; }
    session_bytes = (double)text.len;
    pb_free(&text);
    if (session_bytes > PB_MAX_SESSION_BYTES) {
        pb_failf(f, "ValueError", "session exceeds the import size limit");
        return NULL;
    }
    *total_bytes += session_bytes;
    if (*total_bytes > PB_MAX_TOTAL_BYTES) {
        pb_failf(f, "ValueError", "import exceeds the total size limit");
        return NULL;
    }
    item = pb_core_normalize_session(raw, sid, messages, f);
    if (!item) return NULL;
    *total_messages += (double)json_len(json_obj_get(item, "messages"));
    if (*total_messages > PB_MAX_TOTAL_MESSAGES) {
        json_free(item);
        pb_failf(f, "ValueError", "messages exceeds the total import limit");
        return NULL;
    }
    return item;
}

static json_t *pb_core_validate_payload(const json_t *sessions, pb_fail *f) {
    json_t *normalized = json_array();
    json_t *errors = json_array();
    json_t *seen = json_array();
    json_t *pair;
    double total_bytes = 0.0, total_messages = 0.0;
    size_t i;

    if (!sessions || sessions->type != JSON_ARRAY) {
        pb_failf(f, "TypeError", "sessions must be a list");
        json_free(normalized);
        json_free(errors);
        json_free(seen);
        return NULL;
    }
    for (i = 0; i < json_len(sessions); i++) {
        const json_t *raw = json_get(sessions, i);
        pb_buf sid;
        pb_fail inner;
        json_t *item;
        pb_init(&sid);
        if (raw && raw->type == JSON_OBJECT) {
            const json_t *idv = json_obj_get(raw, "id");
            if (pb_truthy(idv)) pb_py_str(&sid, idv);
            pb_strip(&sid);
        }
        inner.kind = NULL;
        pb_init(&inner.msg);
        item = pb_core_validate_session(raw, pb_cstr(&sid), seen, &total_bytes, &total_messages, &inner);
        if (!item) {
            if (inner.kind && strcmp(inner.kind, "ValueError") != 0) {
                /* Only ValueError is a per-entry rejection; anything else
                 * escapes the whole import, as upstream does. */
                pb_failf(f, inner.kind, "%s", pb_cstr(&inner.msg));
                pb_free(&inner.msg);
                pb_free(&sid);
                json_free(normalized);
                json_free(errors);
                json_free(seen);
                return NULL;
            }
            {
                json_t *err = json_object();
                json_set(err, "index", json_number((double)i));
                json_set(err, "error", json_string(pb_cstr(&inner.msg)));
                if (sid.len) json_set(err, "session_id", json_string(pb_cstr(&sid)));
                json_append(errors, err);
            }
            pb_free(&inner.msg);
            pb_free(&sid);
            continue;
        }
        pb_free(&inner.msg);
        json_append(seen, json_string(pb_cstr(&sid)));
        pb_free(&sid);
        {
            json_t *wrapped = json_object();
            json_set(wrapped, "index", json_number((double)i));
            json_set(wrapped, "session", json_copy(json_obj_get(item, "session")));
            json_set(wrapped, "messages", json_copy(json_obj_get(item, "messages")));
            json_free(item);
            json_append(normalized, wrapped);
        }
    }
    json_free(seen);
    pair = json_array();
    json_append(pair, normalized);
    json_append(pair, errors);
    return pair;
}

/* ---- foreign import match ----------------------------------------------- */

static json_t *pb_core_foreign_match(const json_t *rows, const json_t *origin, pb_fail *f) {
    const json_t *tool;
    size_t i;

    if (!origin || origin->type != JSON_OBJECT) {
        pb_failf(f, "TypeError", "origin must be an object");
        return NULL;
    }
    tool = json_obj_get(origin, "tool");
    if (!tool) {
        pb_failf(f, "KeyError", "'tool'");
        return NULL;
    }
    if (!rows || rows->type != JSON_ARRAY) return json_null();
    for (i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        const json_t *raw = row && row->type == JSON_OBJECT ? json_obj_get(row, "origin_json") : NULL;
        json_t *parsed;
        const json_t *imported, *foreign;
        int has_foreign;
        if (!raw || raw->type == JSON_NULL) continue;
        if (raw->type != JSON_STRING) continue;   /* safe_json_loads: TypeError -> {} */
        parsed = json_parse(raw->str_val ? raw->str_val : "", NULL);
        if (!parsed) continue;                     /* default {} */
        if (!pb_truthy(parsed)) { json_free(parsed); continue; }
        if (parsed->type != JSON_OBJECT) {
            pb_failf(f, "AttributeError", "'%s' object has no attribute 'get'",
                     parsed->type == JSON_ARRAY ? "list" : "str");
            json_free(parsed);
            return NULL;
        }
        imported = json_obj_get(parsed, "imported_from");
        if (!pb_truthy(imported)) imported = NULL;
        {
            const json_t *itool = imported && imported->type == JSON_OBJECT
                                      ? json_obj_get(imported, "tool") : NULL;
            if (!pb_equal(itool, tool)) { json_free(parsed); continue; }
        }
        foreign = json_obj_get(origin, "foreign_session_id");
        has_foreign = pb_truthy(foreign);
        if (has_foreign
            && pb_equal(foreign, json_obj_get(imported, "foreign_session_id"))) {
            json_t *hit = json_copy(json_obj_get(row, "id"));
            json_free(parsed);
            return hit;
        }
        if (!has_foreign) {
            const json_t *path = json_obj_get(origin, "path");
            if (!path) {
                pb_failf(f, "KeyError", "'path'");
                json_free(parsed);
                return NULL;
            }
            if (pb_equal(json_obj_get(imported, "path"), path)) {
                json_t *hit = json_copy(json_obj_get(row, "id"));
                json_free(parsed);
                return hit;
            }
        }
        json_free(parsed);
    }
    return json_null();
}

/* ---- parent re-attachment ----------------------------------------------- */

typedef struct { char *key; char *val; } pb_kv;
typedef struct { pb_kv *items; size_t n, cap; } pb_map;

static char *pb_strdup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

static void pb_map_set(pb_map *m, const char *k, const char *v) {
    size_t i;
    for (i = 0; i < m->n; i++) {
        if (!strcmp(m->items[i].key, k)) {
            free(m->items[i].val);
            m->items[i].val = pb_strdup(v);
            return;
        }
    }
    if (m->n == m->cap) {
        size_t cap = m->cap ? m->cap * 2 : 16;
        pb_kv *ni = (pb_kv *)realloc(m->items, cap * sizeof(*ni));
        if (!ni) return;
        m->items = ni;
        m->cap = cap;
    }
    m->items[m->n].key = pb_strdup(k);
    m->items[m->n].val = pb_strdup(v);
    m->n++;
}

static const char *pb_map_get(const pb_map *m, const char *k) {
    size_t i;
    for (i = 0; i < m->n; i++)
        if (!strcmp(m->items[i].key, k)) return m->items[i].val;
    return NULL;
}

static void pb_map_del(pb_map *m, const char *k) {
    size_t i;
    for (i = 0; i < m->n; i++) {
        if (!strcmp(m->items[i].key, k)) {
            free(m->items[i].key);
            free(m->items[i].val);
            memmove(m->items + i, m->items + i + 1, (m->n - i - 1) * sizeof(*m->items));
            m->n--;
            return;
        }
    }
}

static void pb_map_free(pb_map *m) {
    size_t i;
    for (i = 0; i < m->n; i++) { free(m->items[i].key); free(m->items[i].val); }
    free(m->items);
    m->items = NULL;
    m->n = m->cap = 0;
}

static const json_t *pb_store_row(const json_t *rows, const char *id) {
    size_t i;
    if (!rows || rows->type != JSON_ARRAY) return NULL;
    for (i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        const json_t *rid = row && row->type == JSON_OBJECT ? json_obj_get(row, "id") : NULL;
        if (rid && rid->type == JSON_STRING && !strcmp(rid->str_val ? rid->str_val : "", id)) return row;
    }
    return NULL;
}

/* seen set as a growable string array */
typedef struct { char **items; size_t n, cap; } pb_set;

static int pb_set_add(pb_set *s, const char *v) {
    size_t i;
    for (i = 0; i < s->n; i++) if (!strcmp(s->items[i], v)) return 0;
    if (s->n == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 16;
        char **ni = (char **)realloc(s->items, cap * sizeof(*ni));
        if (!ni) return 0;
        s->items = ni;
        s->cap = cap;
    }
    s->items[s->n] = pb_strdup(v);
    s->n++;
    return 1;
}

static void pb_set_free(pb_set *s) {
    size_t i;
    for (i = 0; i < s->n; i++) free(s->items[i]);
    free(s->items);
    s->items = NULL;
    s->n = s->cap = 0;
}

static int pb_would_cycle(const char *session_id, const char *parent_id, pb_map *map,
                          const json_t *rows, size_t bound) {
    pb_set seen;
    const char *current = parent_id;
    size_t steps = 0;
    int cycle = 0;
    seen.items = NULL;
    seen.n = 0;
    seen.cap = 0;
    pb_set_add(&seen, session_id);
    while (current && current[0]) {
        const char *edge;
        if (pb_set_add(&seen, current) == 0) { cycle = 1; break; }
        if (++steps > bound) { cycle = 1; break; }
        edge = pb_map_get(map, current);
        if (edge) { current = edge; continue; }
        {
            const json_t *row = pb_store_row(rows, current);
            const json_t *p;
            if (!row) { cycle = 0; break; }
            p = json_obj_get(row, "parent_session_id");
            if (!p || p->type != JSON_STRING) { cycle = 0; break; }
            current = p->str_val;
        }
    }
    pb_set_free(&seen);
    return cycle;
}

static json_t *pb_core_parent_attach(const json_t *updates, const json_t *rows, pb_fail *f) {
    pb_map map;
    json_t *kept = json_array();
    json_t *out;
    size_t i, bound;
    int detached = 0;

    map.items = NULL;
    map.n = 0;
    map.cap = 0;
    if (!updates || updates->type != JSON_ARRAY) {
        pb_failf(f, "TypeError", "parent_updates must be a list");
        json_free(kept);
        return NULL;
    }
    for (i = 0; i < json_len(updates); i++) {
        const json_t *pair = json_get(updates, i);
        const json_t *sid = pair && pair->type == JSON_ARRAY ? json_get(pair, 0) : NULL;
        const json_t *pid = pair && pair->type == JSON_ARRAY ? json_get(pair, 1) : NULL;
        if (!sid || sid->type != JSON_STRING || !pid || pid->type != JSON_STRING) {
            pb_failf(f, "TypeError", "parent_updates entries must be string pairs");
            pb_map_free(&map);
            json_free(kept);
            return NULL;
        }
        pb_map_set(&map, sid->str_val, pid->str_val);
    }
    bound = json_len(updates) + (rows && rows->type == JSON_ARRAY ? json_len(rows) : 0) + 2;
    for (i = 0; i < json_len(updates); i++) {
        const json_t *pair = json_get(updates, i);
        const char *sid = json_get(pair, 0)->str_val;
        const char *pid = json_get(pair, 1)->str_val;
        const json_t *store = pb_store_row(rows, pid);
        if (store && !pb_would_cycle(sid, pid, &map, rows, bound)) {
            json_t *row = json_array();
            json_append(row, json_string(sid));
            json_append(row, json_string(pid));
            json_append(kept, row);
        } else {
            pb_map_del(&map, sid);
            detached++;
        }
    }
    pb_map_free(&map);
    out = json_object();
    json_set(out, "updates", kept);
    json_set(out, "detached", json_number((double)detached));
    return out;
}

/* ---- row-writer mask ---------------------------------------------------- */

static json_t *pb_core_import_flags(const json_t *raw, pb_fail *f) {
    json_t *out;
    double flag[PB_FLAG_COLS_N];
    size_t i;

    if (!raw || raw->type != JSON_OBJECT) {
        pb_failf(f, "TypeError", "session must be an object");
        return NULL;
    }
    for (i = 0; i < PB_FLAG_COLS_N; i++) {
        double d = 0.0;
        int r = pb_coerce_int(json_obj_get(raw, PB_FLAG_COLS[i]), &d, f);
        if (r < 0) return NULL;
        flag[i] = (r == 1) ? d : 0.0;
    }
    out = json_object();
    /* Only the sweep sets auto_archived, and only on rows it archives. */
    if (flag[1] != 0.0 && flag[0] == 0.0) flag[1] = 0.0;
    for (i = 0; i < PB_FLAG_COLS_N; i++)
        json_set(out, PB_FLAG_COLS[i], json_number(flag[i] != 0.0 ? 1.0 : 0.0));
    return out;
}

static json_t *pb_core_message_split(const json_t *messages, pb_fail *f) {
    json_t *out = json_object();
    json_t *compacted = json_array();
    double live = 0.0, archived = 0.0;
    size_t i;
    if (!messages || messages->type != JSON_ARRAY) {
        pb_failf(f, "TypeError", "messages must be a list");
        json_free(compacted);
        json_free(out);
        return NULL;
    }
    for (i = 0; i < json_len(messages); i++) {
        const json_t *msg = json_get(messages, i);
        double active = 1.0, comp = 0.0;
        int r;
        if (!msg || msg->type != JSON_OBJECT) {
            pb_failf(f, "TypeError", "messages must contain only objects");
            json_free(compacted);
            json_free(out);
            return NULL;
        }
        r = pb_coerce_int(json_obj_get(msg, "active"), &active, f);
        if (r < 0) { json_free(compacted); json_free(out); return NULL; }
        if (r == 0) active = 1.0;                       /* _coerce_or(..., int, 1) */
        if (active != 0.0) { live += 1.0; continue; }
        archived += 1.0;
        r = pb_coerce_int(json_obj_get(msg, "compacted"), &comp, f);
        if (r < 0) { json_free(compacted); json_free(out); return NULL; }
        if (r == 0) comp = 0.0;
        json_append(compacted, json_number(comp != 0.0 ? 1.0 : 0.0));
    }
    json_set(out, "live", json_number(live));
    json_set(out, "archived", json_number(archived));
    json_set(out, "compacted", compacted);
    return out;
}

/* ---- export row --------------------------------------------------------- */

static json_t *pb_core_export_row(const json_t *session, pb_fail *f) {
    json_t *out = json_object();
    size_t i;
    /* The record's own field names: the row dicts carry the private spellings. */
    static const struct { const char *src; const char *dst; } EXTRA[3] = {
        {"last_active", "last_active"}, {"_reset_from", "reset_from"},
        {"_branched_from", "branched_from"},
    };
    if (!session || session->type != JSON_OBJECT) {
        pb_failf(f, "TypeError", "session must be an object");
        json_free(out);
        return NULL;
    }
    for (i = 0; i < PB_EXPORT_COLS_N; i++) {
        const json_t *v = json_obj_get(session, PB_EXPORT_COLS[i]);
        if (!v) {
            pb_failf(f, "KeyError", "'%s'", PB_EXPORT_COLS[i]);
            json_free(out);
            return NULL;
        }
        json_set(out, PB_EXPORT_COLS[i], json_copy(v));
    }
    for (i = 0; i < 3; i++) {
        const json_t *v = json_obj_get(session, EXTRA[i].src);
        json_set(out, EXTRA[i].dst, v ? json_copy(v) : json_null());
    }
    return out;
}

/* ======================================================================= */
/* public entry points                                                     */
/* ======================================================================= */

static json_t *pb_str_array(const char *const *items, size_t n) {
    json_t *a = json_array();
    size_t i;
    for (i = 0; i < n; i++) json_append(a, json_string(items[i]));
    return a;
}

json_t *w33_portabilit_b54bb9_constants(void) {
    json_t *o = json_object();
    json_set(o, "insert_sql", json_string(PB_INSERT_SQL));
    json_set(o, "session_text_fields", pb_str_array(PB_SESSION_TEXT, PB_SESSION_TEXT_N));
    json_set(o, "message_text_fields", pb_str_array(PB_MESSAGE_TEXT, PB_MESSAGE_TEXT_N));
    json_set(o, "message_json_fields", pb_str_array(PB_MESSAGE_JSON, PB_MESSAGE_JSON_N));
    json_set(o, "passthrough_cols", pb_str_array(PB_PASSTHROUGH, PB_PASSTHROUGH_N));
    json_set(o, "int_cols", pb_str_array(PB_INT_COLS, PB_INT_COLS_N));
    json_set(o, "float_cols", pb_str_array(PB_FLOAT_COLS, PB_FLOAT_COLS_N));
    json_set(o, "flag_cols", pb_str_array(PB_FLAG_COLS, PB_FLAG_COLS_N));
    json_set(o, "export_columns", pb_str_array(PB_EXPORT_COLS, PB_EXPORT_COLS_N));
    json_set(o, "max_sessions", json_number(PB_MAX_SESSIONS));
    json_set(o, "max_messages_per_session", json_number(PB_MAX_MESSAGES_PER_SESSION));
    json_set(o, "max_total_messages", json_number(PB_MAX_TOTAL_MESSAGES));
    json_set(o, "max_session_bytes", json_number(PB_MAX_SESSION_BYTES));
    json_set(o, "max_total_bytes", json_number(PB_MAX_TOTAL_BYTES));
    json_set(o, "sqlite_int_min", json_string(PB_SQLITE_INT_MIN_TEXT));
    json_set(o, "sqlite_int_max", json_string(PB_SQLITE_INT_MAX_TEXT));
    json_set(o, "epoch_min", json_number(PB_EPOCH_MIN));
    json_set(o, "epoch_max", json_number(PB_EPOCH_MAX));
    return pb_ok(o);
}

json_t *w33_portabilit_b54bb9_export_row(const json_t *session) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_export_row(session, &f), &f);
}

json_t *w33_portabilit_b54bb9_text_or_none(const json_t *value, const char *field) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_text_or_none(value, field, &f), &f);
}

json_t *w33_portabilit_b54bb9_int_or_none(const json_t *value, const char *field) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_int_or_none(value, field, &f), &f);
}

json_t *w33_portabilit_b54bb9_int_or_bounded(const json_t *value, const char *field,
                                            const json_t *fallback) {
    pb_fail f;
    json_t *zero;
    f.kind = NULL;
    pb_init(&f.msg);
    if (fallback) return pb_finish(pb_core_int_or_bounded(value, field, fallback, &f), &f);
    zero = json_number(0.0);
    {
        json_t *v = pb_core_int_or_bounded(value, field, zero, &f);
        json_free(zero);
        return pb_finish(v, &f);
    }
}

json_t *w33_portabilit_b54bb9_float_or_finite(const json_t *value, const char *field) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_float_or_finite(value, field, &f), &f);
}

json_t *w33_portabilit_b54bb9_json_object_or_none(const json_t *value, const char *field) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_json_object_or_none(value, field, &f), &f);
}

json_t *w33_portabilit_b54bb9_non_finite_path(const json_t *value, const char *path) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_non_finite_path(value, path ? path : "", &f), &f);
}

json_t *w33_portabilit_b54bb9_load_json(const char *text, const char *path) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_load_json(text, path ? path : "", &f), &f);
}

json_t *w33_portabilit_b54bb9_message_content(const json_t *content, const char *path) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_message_content(content, path ? path : "", &f), &f);
}

json_t *w33_portabilit_b54bb9_normalize_message(const json_t *message, int index) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_normalize_message(message, index, &f), &f);
}

json_t *w33_portabilit_b54bb9_normalize_session(const json_t *raw, const char *session_id,
                                               const json_t *messages) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_normalize_session(raw, session_id ? session_id : "", messages, &f), &f);
}

json_t *w33_portabilit_b54bb9_validate_payload(const json_t *sessions) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_validate_payload(sessions, &f), &f);
}

json_t *w33_portabilit_b54bb9_foreign_match(const json_t *rows, const json_t *origin) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_foreign_match(rows, origin, &f), &f);
}

json_t *w33_portabilit_b54bb9_parent_attach(const json_t *parent_updates,
                                           const json_t *store_rows) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_parent_attach(parent_updates, store_rows, &f), &f);
}

json_t *w33_portabilit_b54bb9_import_flags(const json_t *raw) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_import_flags(raw, &f), &f);
}

json_t *w33_portabilit_b54bb9_message_split(const json_t *messages) {
    pb_fail f;
    f.kind = NULL;
    pb_init(&f.msg);
    return pb_finish(pb_core_message_split(messages, &f), &f);
}
