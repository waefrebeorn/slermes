/* port_kanban_db_flow.c — FLOW-core port of hermes_cli/kanban_db.py (storage layer).
 *
 * Ports the task-CRUD decision sequences: edit/backfill fences, claim CAS +
 * run opening, block routing, completion gates, stale-claim extension,
 * override writes, reclaim bookkeeping, relation deletes, and the read-side
 * leaves (status/run-id probes, dependency queries, reviewer provenance,
 * skills/model validation, claim liveness). All SQLite I/O is INJECTED through
 * the kdbf_io_t seam dispatcher (scripted cursor results / row objects /
 * exception flags) — no real sqlite is linked. SQL strings are byte-pinned to
 * the live source via the GENERATED kdbf_flow_sql.h.
 *
 * Seam model (mirrors port_scheduler_flow.c): every conn.execute / cursor
 * fetch / write_txn boundary / already-ported I/O sibling (_append_event,
 * _end_run, _synthesize_ended_run, notify_task_updated, _to_epoch,
 * _resolve_claim_ttl_seconds, _worker_alive, get_toolset_names) goes through
 * kdbf_seam(), which appends a structured event {"call": name, ...args} to
 * io->trace (ordered decision sequence) and returns the scripted result.
 * A raised exception is modeled as {"__raise__": "<Type>", "message": "..."}.
 * Pure internal helpers (_first_line, _substantive_text, _json_or, _json_dict,
 * _row_get, _json_or_null, _cleaned_artifact_paths, _latest_event) stay REAL
 * here and in the oracle (AST-extracted), so nested logic is exercised.
 *
 * Deterministic subset (documented): str.strip/splitlines/casefold cover ASCII
 * plus the common Unicode whitespace/line-separator code points; non-ASCII
 * casefold specials (e.g. sharp-s) and naive-ISO _to_epoch values are out of
 * fixture scope (_to_epoch is a seam). JSON numbers lose int/float identity
 * after parsing: integral doubles serialize as ints (json.dumps(1.0) would
 * emit "1.0" — fixtures carry ints).
 *
 * Ownership: seam results and function returns are caller-owned (json_free).
 * io->trace / io->state belong to the harness. Args passed to kdbf_seam are
 * consumed (copied into the trace event, then freed). Borrowed inputs are
 * never freed. A NULL json_t* input models Python None.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <inttypes.h>
#include "hermes_json.h"
#include "kdbf_flow_sql.h"

/* Live module constant (kanban_db.py: BLOCK_RECURRENCE_LIMIT = 2). */
#define KDBF_BLOCK_RECURRENCE_LIMIT 2

/* ── seam dispatcher + trace ─────────────────────────────────────────────── */
typedef struct kdbf_io_t {
    json_t *(*call)(struct kdbf_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of event objects: ordered decision sequence */
    json_t *state;   /* JSON object: mutable module globals (unused here) */
    void   *ctx;     /* harness backend (scripted results) */
} kdbf_io_t;

static bool kdbf_raised(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static json_t *kdbf_raise(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}

/* Build a sentinel from a scripted {"type","message"} raise spec. */
static json_t *kdbf_raise_from_spec(const json_t *spec) {
    const char *t = json_get_str(spec, "type", "Exception");
    const char *m = json_get_str(spec, "message", "");
    return kdbf_raise(t, m);
}

/* Record {"call": name, **args} and dispatch; consumes args. */
static json_t *kdbf_seam(kdbf_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "call", json_string(name));
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++)
            json_set(ev, args->c.keys[i], json_copy(args->c.items[i]));
    }
    if (io && io->trace && io->trace->type == JSON_ARRAY)
        json_append(io->trace, ev);
    else
        json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : json_null();
    if (args) json_free(args);
    return r ? r : json_null();
}

/* ── Python-faithful micro helpers ───────────────────────────────────────── */
static bool kdbf_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static bool kdbf_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

/* UTF-8 code point length (Python len(str)). */
static size_t kdbf_py_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* Decode one UTF-8 code point at *p; advances *p. Returns the code point. */
static unsigned long kdbf_next_cp(const unsigned char **p) {
    const unsigned char *s = *p;
    unsigned long cp = s[0];
    int extra = 0;
    if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
    else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
    else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
    for (int i = 1; i <= extra && s[i]; i++)
        cp = (cp << 6) | (s[i] & 0x3F);
    *p = s + 1 + extra;
    return cp;
}

/* Python str.isspace() set (ASCII + the Unicode whitespace code points). */
static bool kdbf_is_space_cp(unsigned long cp) {
    if (cp <= 0x7F)
        return (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F) || cp == 0x20;
    return cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 ||
           cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

/* Python str.strip() (both ends). Caller frees. */
static char *kdbf_strip(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *a = p, *b = p + strlen((const char *)p);
    while (a < b) {
        const unsigned char *q = a;
        unsigned long cp = kdbf_next_cp(&q);
        if (!kdbf_is_space_cp(cp)) break;
        a = q;
    }
    while (b > a) {
        /* walk back one UTF-8 sequence */
        const unsigned char *q = b - 1;
        while (q > a && (*q & 0xC0) == 0x80) q--;
        const unsigned char *r = q;
        unsigned long cp = kdbf_next_cp(&r);
        if (!kdbf_is_space_cp(cp) || r != b) break;
        b = q;
    }
    size_t n = (size_t)(b - a);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* Python str.splitlines() boundary test at p (ASCII + U+0085/2028/2029).
 * Returns the byte width of the separator, 0 when none. */
static size_t kdbf_line_sep(const unsigned char *p) {
    if (p[0] == '\r') return (p[1] == '\n') ? 2 : 1;
    if (p[0] == '\n' || p[0] == '\v' || p[0] == '\f' ||
        p[0] == 0x1C || p[0] == 0x1D || p[0] == 0x1E) return 1;
    if (p[0] == 0xC2 && p[1] == 0x85) return 2;
    if (p[0] == 0xE2 && p[1] == 0x80 && (p[2] == 0xA8 || p[2] == 0xA9)) return 3;
    return 0;
}

/* _first_line: first stripped line capped at limit CODE POINTS; "" when empty.
 * Caller frees. */
static char *kdbf_first_line(const json_t *text, size_t limit) {
    const char *raw = (text && text->type == JSON_STRING && text->str_val) ? text->str_val : "";
    char *stripped = kdbf_strip(raw);
    if (!stripped) return NULL;
    const unsigned char *p = (const unsigned char *)stripped;
    const unsigned char *end = p + strlen(stripped);
    const unsigned char *line_end = p;
    while (line_end < end && !kdbf_line_sep(line_end)) line_end++;
    /* cap at limit code points */
    const unsigned char *q = p;
    size_t chars = 0;
    while (q < line_end && chars < limit) {
        kdbf_next_cp(&q);
        chars++;
    }
    size_t n = (size_t)(q - p);
    char *out = malloc(n + 1);
    if (!out) { free(stripped); return NULL; }
    memcpy(out, p, n);
    out[n] = '\0';
    free(stripped);
    return out;
}

/* _substantive_text: value is not None and str(value).strip() non-empty. */
static bool kdbf_substantive_text(const json_t *v) {
    if (kdbf_is_none(v)) return false;
    if (v->type == JSON_STRING) {
        char *s = kdbf_strip(v->str_val ? v->str_val : "");
        bool nonblank = s && s[0] != '\0';
        free(s);
        return nonblank;
    }
    /* str() of any other value is non-empty (numbers, bools, containers). */
    return true;
}

/* ASCII casefold (== lower for ASCII; non-ASCII specials out of fixture scope). */
static char *kdbf_casefold(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

/* ── string builder ──────────────────────────────────────────────────────── */
typedef struct { char *p; size_t len, cap; } kdbf_sb_t;

static void kdbf_sb_init(kdbf_sb_t *sb) { sb->p = NULL; sb->len = 0; sb->cap = 0; }

static void kdbf_sb_add(kdbf_sb_t *sb, const char *s) {
    size_t n = strlen(s);
    if (sb->len + n + 1 > sb->cap) {
        size_t nc = (sb->len + n + 1) * 2;
        char *np = realloc(sb->p, nc);
        if (!np) return;
        sb->p = np; sb->cap = nc;
    }
    memcpy(sb->p + sb->len, s, n);
    sb->len += n;
    sb->p[sb->len] = '\0';
}

static void kdbf_sb_free(kdbf_sb_t *sb) { free(sb->p); sb->p = NULL; sb->len = sb->cap = 0; }

/* ── Python repr(str) / str(number) / json.dumps(ensure_ascii=False) ─────── */

/* Shortest round-trip float repr, formatted like Python repr(float). */
static void kdbf_fmt_float(kdbf_sb_t *sb, double v) {
    char buf[64];
    if (isnan(v)) { kdbf_sb_add(sb, "NaN"); return; }
    if (isinf(v)) { kdbf_sb_add(sb, v < 0 ? "-Infinity" : "Infinity"); return; }
    if (v == 0.0) { kdbf_sb_add(sb, signbit(v) ? "-0.0" : "0.0"); return; }
    bool neg = v < 0;
    double a = neg ? -v : v;
    int prec;
    for (prec = 0; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*e", prec, a);
        if (strtod(buf, NULL) == a) break;
    }
    if (prec > 17) prec = 17;
    /* buf = d[.ddd]e±XX */
    char digits[32];
    size_t nd = 0;
    const char *e = strchr(buf, 'e');
    for (const char *q = buf; q < e; q++)
        if (*q != '.') digits[nd++] = *q;
    digits[nd] = '\0';
    int exp = atoi(e + 1);
    if (neg) kdbf_sb_add(sb, "-");
    if (exp >= -4 && exp < 16) {
        /* fixed notation */
        if (exp >= 0) {
            size_t intd = (size_t)exp + 1;
            if (nd <= intd) {
                kdbf_sb_add(sb, digits);
                for (size_t i = nd; i < intd; i++) kdbf_sb_add(sb, "0");
                kdbf_sb_add(sb, ".0");
            } else {
                char tmp[64];
                memcpy(tmp, digits, intd);
                tmp[intd] = '\0';
                kdbf_sb_add(sb, tmp);
                kdbf_sb_add(sb, ".");
                kdbf_sb_add(sb, digits + intd);
            }
        } else {
            kdbf_sb_add(sb, "0.");
            for (int i = 0; i < -exp - 1; i++) kdbf_sb_add(sb, "0");
            kdbf_sb_add(sb, digits);
        }
    } else {
        char tmp[8];
        tmp[0] = digits[0]; tmp[1] = '\0';
        kdbf_sb_add(sb, tmp);
        if (nd > 1) {
            kdbf_sb_add(sb, ".");
            kdbf_sb_add(sb, digits + 1);
        }
        snprintf(buf, sizeof buf, "e%c%02d", exp < 0 ? '-' : '+', exp < 0 ? -exp : exp);
        kdbf_sb_add(sb, buf);
    }
}

/* Python str() of a JSON scalar (numbers integral -> int form). */
static void kdbf_fmt_number(kdbf_sb_t *sb, double v) {
    if (v == trunc(v) && fabs(v) < 9.2e18) {
        char buf[32];
        snprintf(buf, sizeof buf, "%lld", (long long)v);
        kdbf_sb_add(sb, buf);
    } else {
        kdbf_fmt_float(sb, v);
    }
}

/* Python repr() of a str value (quote choice + escapes). Caller frees. */
static char *kdbf_py_repr_str(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    kdbf_sb_t sb;
    kdbf_sb_init(&sb);
    char q[2] = { quote, '\0' };
    kdbf_sb_add(&sb, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)quote || c == '\\') {
            char esc[3] = { '\\', (char)c, '\0' };
            kdbf_sb_add(&sb, esc);
        } else if (c == '\n') kdbf_sb_add(&sb, "\\n");
        else if (c == '\r') kdbf_sb_add(&sb, "\\r");
        else if (c == '\t') kdbf_sb_add(&sb, "\\t");
        else if (c < 0x20 || c == 0x7F) {
            char esc[8];
            snprintf(esc, sizeof esc, "\\x%02x", c);
            kdbf_sb_add(&sb, esc);
        } else {
            char ch[2] = { (char)c, '\0' };
            kdbf_sb_add(&sb, ch);
        }
    }
    kdbf_sb_add(&sb, q);
    return sb.p ? sb.p : strdup("");
}

/* json.dumps(obj, ensure_ascii=False) — default separators (", ", ": "),
 * insertion-ordered keys, integral doubles as ints. Caller frees. */
static void kdbf_dumps_rec(kdbf_sb_t *sb, const json_t *v) {
    if (!v || v->type == JSON_NULL) { kdbf_sb_add(sb, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: kdbf_sb_add(sb, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: kdbf_fmt_number(sb, v->num_val); break;
        case JSON_STRING: {
            kdbf_sb_add(sb, "\"");
            for (const unsigned char *p = (const unsigned char *)(v->str_val ? v->str_val : ""); *p; p++) {
                unsigned char c = *p;
                if (c == '"') kdbf_sb_add(sb, "\\\"");
                else if (c == '\\') kdbf_sb_add(sb, "\\\\");
                else if (c == '\n') kdbf_sb_add(sb, "\\n");
                else if (c == '\r') kdbf_sb_add(sb, "\\r");
                else if (c == '\t') kdbf_sb_add(sb, "\\t");
                else if (c == '\b') kdbf_sb_add(sb, "\\b");
                else if (c == '\f') kdbf_sb_add(sb, "\\f");
                else if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof esc, "\\u%04x", c);
                    kdbf_sb_add(sb, esc);
                } else {
                    char ch[2] = { (char)c, '\0' };
                    kdbf_sb_add(sb, ch);
                }
            }
            kdbf_sb_add(sb, "\"");
            break;
        }
        case JSON_ARRAY:
            kdbf_sb_add(sb, "[");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) kdbf_sb_add(sb, ", ");
                kdbf_dumps_rec(sb, v->c.items[i]);
            }
            kdbf_sb_add(sb, "]");
            break;
        case JSON_OBJECT:
            kdbf_sb_add(sb, "{");
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) kdbf_sb_add(sb, ", ");
                json_t *k = json_string(v->c.keys[i]);
                kdbf_dumps_rec(sb, k);
                json_free(k);
                kdbf_sb_add(sb, ": ");
                kdbf_dumps_rec(sb, v->c.items[i]);
            }
            kdbf_sb_add(sb, "}");
            break;
        default: kdbf_sb_add(sb, "null"); break;
    }
}

static char *kdbf_py_dumps(const json_t *v) {
    kdbf_sb_t sb;
    kdbf_sb_init(&sb);
    kdbf_dumps_rec(&sb, v);
    return sb.p ? sb.p : strdup("null");
}

/* _json_or_null: JSON text for a payload column; falsy -> NULL. */
static json_t *kdbf_json_or_null(const json_t *obj) {
    if (!kdbf_truthy(obj)) return json_null();
    char *s = kdbf_py_dumps(obj);
    json_t *r = json_string(s ? s : "null");
    free(s);
    return r;
}

/* _json_or: decode a JSON text column; falsy / decode failure -> default. */
static json_t *kdbf_json_or(const json_t *value, const json_t *deflt) {
    if (!kdbf_truthy(value)) return json_copy(deflt);
    if (value->type != JSON_STRING) return json_copy(deflt);
    char *err = NULL;
    json_t *parsed = json_parse(value->str_val ? value->str_val : "", &err);
    free(err);
    if (!parsed) return json_copy(deflt);
    return parsed;
}

/* _json_dict: decoded object or {}. */
static json_t *kdbf_json_dict(const json_t *value) {
    json_t *empty = json_object();
    json_t *parsed = kdbf_json_or(value, empty);
    json_free(empty);
    if (parsed && parsed->type == JSON_OBJECT) return parsed;
    json_free(parsed);
    return json_object();
}

/* _row_get: row[col] tolerant of an absent column / None row. */
static json_t *kdbf_row_get(const json_t *row, const char *col, const json_t *deflt) {
    if (kdbf_is_none(row) || row->type != JSON_OBJECT || !json_has(row, col))
        return json_copy(deflt);
    return json_copy(json_obj_get(row, col));
}

/* Direct row[col] — KeyError when the column is absent (fail closed). */
static json_t *kdbf_row_col(const json_t *row, const char *col) {
    if (!row || row->type != JSON_OBJECT || !json_has(row, col)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s'", col);
        return kdbf_raise("KeyError", msg);
    }
    return json_copy(json_obj_get(row, col));
}

/* Python int(value) on a JSON scalar; ValueError/TypeError sentinel on failure. */
static json_t *kdbf_py_int_node(const json_t *v) {
    if (kdbf_is_none(v))
        return kdbf_raise("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) return json_number(trunc(v->num_val));
    if (v->type == JSON_STRING) {
        const char *raw = v->str_val ? v->str_val : "";
        char *s = kdbf_strip(raw);
        bool ok = s && s[0] != '\0';
        long long acc = 0;
        if (ok) {
            const char *p = s;
            bool neg = false;
            if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
            const char *digits_start = p;
            bool prev_digit = false, any = false;
            for (; *p; p++) {
                if (*p >= '0' && *p <= '9') {
                    acc = acc * 10 + (*p - '0');
                    prev_digit = true; any = true;
                } else if (*p == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') {
                    prev_digit = false;
                } else { ok = false; break; }
            }
            if (!any || (p > digits_start && p[-1] == '_')) ok = false;
            if (neg) acc = -acc;
        }
        if (ok) {
            free(s);
            return json_number((double)acc);
        }
        free(s);
        char *rep = kdbf_py_repr_str(raw);
        char msg[512];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", rep);
        free(rep);
        return kdbf_raise("ValueError", msg);
    }
    const char *tn = (v->type == JSON_ARRAY) ? "list" : "dict";
    char msg[160];
    snprintf(msg, sizeof msg,
        "int() argument must be a string, a bytes-like object or a real number, not '%s'", tn);
    return kdbf_raise("TypeError", msg);
}

/* Python str() of a scalar node (skills normalization). Caller frees. */
static char *kdbf_py_str_node(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        kdbf_sb_t sb;
        kdbf_sb_init(&sb);
        kdbf_fmt_number(&sb, v->num_val);
        return sb.p ? sb.p : strdup("");
    }
    /* containers: repr-ish fallback (out of fixture scope) */
    return kdbf_py_dumps(v);
}

/* ── scripted cursor model ───────────────────────────────────────────────── */
typedef struct { json_t *spec; size_t idx; } kdbf_cursor_t;

static void kdbf_cursor_clear(kdbf_cursor_t *cur) {
    if (cur->spec) json_free(cur->spec);
    cur->spec = NULL;
    cur->idx = 0;
}

/* conn.execute(sql, params) — records the verbatim SQL + params, returns a
 * raise sentinel on scripted failure, else fills cur (spec owned by cur). */
static json_t *kdbf_execute(kdbf_io_t *io, const char *sql, json_t *params, kdbf_cursor_t *cur) {
    json_t *args = json_object();
    json_set(args, "sql", json_string(sql));
    json_set(args, "params", params ? params : json_array());
    json_t *r = kdbf_seam(io, "execute", args);
    cur->spec = NULL;
    cur->idx = 0;
    if (kdbf_raised(r)) return r;
    cur->spec = r;
    return NULL;
}

static json_t *kdbf_fetch_raise(const kdbf_cursor_t *cur) {
    if (!cur->spec || cur->spec->type != JSON_OBJECT) return NULL;
    json_t *fr = json_obj_get(cur->spec, "fetch_raise");
    if (!fr || fr->type != JSON_OBJECT) return NULL;
    return kdbf_raise_from_spec(fr);
}

static json_t *kdbf_fetchone(kdbf_io_t *io, kdbf_cursor_t *cur) {
    json_t *ev = json_object();
    json_set(ev, "call", json_string("fetchone"));
    json_append(io->trace, ev);
    json_t *fr = kdbf_fetch_raise(cur);
    if (fr) return fr;
    json_t *rows = (cur->spec && cur->spec->type == JSON_OBJECT) ? json_obj_get(cur->spec, "rows") : NULL;
    if (!rows || rows->type != JSON_ARRAY || cur->idx >= json_len(rows)) return json_null();
    return json_copy(json_get(rows, cur->idx++));
}

static json_t *kdbf_fetchall(kdbf_io_t *io, kdbf_cursor_t *cur) {
    json_t *ev = json_object();
    json_set(ev, "call", json_string("fetchall"));
    json_append(io->trace, ev);
    json_t *fr = kdbf_fetch_raise(cur);
    if (fr) return fr;
    json_t *out = json_array();
    json_t *rows = (cur->spec && cur->spec->type == JSON_OBJECT) ? json_obj_get(cur->spec, "rows") : NULL;
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = cur->idx; i < json_len(rows); i++)
            json_append(out, json_copy(json_get(rows, i)));
        cur->idx = json_len(rows);
    }
    return out;
}

static double kdbf_rowcount(const kdbf_cursor_t *cur) {
    if (!cur->spec || cur->spec->type != JSON_OBJECT) return 0;
    return json_get_num(cur->spec, "rowcount", 0);
}

static json_t *kdbf_lastrowid(const kdbf_cursor_t *cur) {
    if (!cur->spec || cur->spec->type != JSON_OBJECT) return json_null();
    json_t *v = json_obj_get(cur->spec, "lastrowid");
    return v ? json_copy(v) : json_null();
}

/* ── write_txn context-manager double ────────────────────────────────────── */
static json_t *kdbf_txn_enter(kdbf_io_t *io, bool allow_nested) {
    json_t *args = json_object();
    json_set(args, "allow_nested", json_bool(allow_nested));
    return kdbf_seam(io, "write_txn.enter", args);
}

/* __exit__: exc is the in-flight raise sentinel (or NULL). Returns the exit
 * seam result — a raise there REPLACES the in-flight exception (Python). */
static json_t *kdbf_txn_exit(kdbf_io_t *io, const json_t *exc) {
    json_t *args = json_object();
    if (kdbf_raised(exc)) {
        json_t *e = json_object();
        json_set(e, "type", json_copy(json_obj_get(exc, "__raise__")));
        json_t *m = json_obj_get(exc, "message");
        json_set(e, "message", m ? json_copy(m) : json_string(""));
        json_set(args, "exc", e);
    } else {
        json_set(args, "exc", json_null());
    }
    return kdbf_seam(io, "write_txn.exit", args);
}

/* Finish a with-block: record __exit__, propagate an exit raise over ret. */
static json_t *kdbf_txn_finish(kdbf_io_t *io, json_t *tx, json_t *ret) {
    json_free(tx);
    json_t *ex = kdbf_txn_exit(io, ret);
    if (kdbf_raised(ex)) {
        json_free(ret);
        return ex;
    }
    json_free(ex);
    return ret;
}

/* ── real internal helpers (unclaimed live defs) ─────────────────────────── */

/* _latest_event: newest task_events row of kind (optionally run-scoped). */
static json_t *kdbf_latest_event(kdbf_io_t *io, const char *task_id, const char *kind,
                                 const json_t *run_id) {
    kdbf_sb_t sql;
    kdbf_sb_init(&sql);
    kdbf_sb_add(&sql, KDBF_SQL_LATEST_BASE);
    json_t *params = json_array();
    json_append(params, json_string(task_id));
    json_append(params, json_string(kind));
    if (!kdbf_is_none(run_id)) {
        kdbf_sb_add(&sql, KDBF_SQL_LATEST_RUN);
        json_t *rid = kdbf_py_int_node(run_id);
        if (kdbf_raised(rid)) {
            kdbf_sb_free(&sql);
            json_free(params);
            return rid;
        }
        json_append(params, rid);
    }
    kdbf_sb_add(&sql, KDBF_SQL_LATEST_ORDER);
    kdbf_cursor_t cur;
    json_t *err = kdbf_execute(io, sql.p ? sql.p : "", params, &cur);
    kdbf_sb_free(&sql);
    if (err) return err;
    json_t *row = kdbf_fetchone(io, &cur);
    kdbf_cursor_clear(&cur);
    return row;
}

/* _cleaned_artifact_paths: non-blank string paths in metadata["artifacts"]. */
static json_t *kdbf_cleaned_artifact_paths(const json_t *metadata) {
    json_t *out = json_array();
    if (!metadata || metadata->type != JSON_OBJECT) return out;
    json_t *raw = json_obj_get(metadata, "artifacts");
    if (!raw || raw->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(raw); i++) {
        json_t *p = json_get(raw, i);
        if (!p || p->type != JSON_STRING) continue;
        char *s = kdbf_strip(p->str_val ? p->str_val : "");
        if (s && s[0] != '\0') json_append(out, json_string(s));
        free(s);
    }
    return out;
}

/* ── read-side leaves ────────────────────────────────────────────────────── */

/* PoP: kdbf_task_status @ hermes_cli/kanban_db.py:_task_status */
json_t *kdbf_task_status(kdbf_io_t *io, const char *task_id) {
    kdbf_cursor_t cur;
    json_t *params = json_array();
    json_append(params, json_string(task_id));
    json_t *err = kdbf_execute(io, KDBF_SQL_TASK_STATUS, params, &cur);
    if (err) return err;
    json_t *row = kdbf_fetchone(io, &cur);
    kdbf_cursor_clear(&cur);
    if (kdbf_raised(row)) return row;
    if (!kdbf_truthy(row)) {
        json_free(row);
        return json_null();
    }
    json_t *st = kdbf_row_col(row, "status");
    json_free(row);
    return st;
}

/* Internal seam-driven helper (live _current_run_id is already claimed by
 * kanban_runs.c:kdb_current_run_id — NOT re-ported here; this copy exists so
 * the flow siblings run under scripted seams, and the oracle still drives
 * the LIVE def for comparison). */
static json_t *kdbf_current_run_id(kdbf_io_t *io, const char *task_id) {
    kdbf_cursor_t cur;
    json_t *params = json_array();
    json_append(params, json_string(task_id));
    json_t *err = kdbf_execute(io, KDBF_SQL_CURRENT_RUN_ID, params, &cur);
    if (err) return err;
    json_t *row = kdbf_fetchone(io, &cur);
    kdbf_cursor_clear(&cur);
    if (kdbf_raised(row)) return row;
    if (!kdbf_truthy(row)) {
        json_free(row);
        return json_null();
    }
    json_t *v = kdbf_row_col(row, "current_run_id");
    json_free(row);
    if (kdbf_raised(v)) return v;
    if (!kdbf_truthy(v)) {
        json_free(v);
        return json_null();
    }
    json_t *r = kdbf_py_int_node(v);
    json_free(v);
    return r;
}

/* PoP: kdbf_extend_run_claim @ hermes_cli/kanban_db.py:_extend_run_claim */
json_t *kdbf_extend_run_claim(kdbf_io_t *io, const char *task_id, double expires) {
    json_t *rid = kdbf_current_run_id(io, task_id);
    if (kdbf_raised(rid)) return rid;
    if (!kdbf_is_none(rid)) {
        kdbf_cursor_t cur;
        json_t *params = json_array();
        json_append(params, json_number(expires));
        json_append(params, json_copy(rid));
        json_t *err = kdbf_execute(io, KDBF_SQL_EXTEND_RUN_CLAIM, params, &cur);
        kdbf_cursor_clear(&cur);
        if (err) {
            json_free(rid);
            return err;
        }
    }
    return rid;
}

/* PoP: kdbf_unsatisfied_parents @ hermes_cli/kanban_db.py:unsatisfied_parents */
json_t *kdbf_unsatisfied_parents(kdbf_io_t *io, const char *task_id) {
    kdbf_cursor_t cur;
    json_t *params = json_array();
    json_append(params, json_string(task_id));
    json_t *err = kdbf_execute(io, KDBF_SQL_UNSATISFIED_PARENTS, params, &cur);
    if (err) return err;
    json_t *rows = kdbf_fetchall(io, &cur);
    kdbf_cursor_clear(&cur);
    if (kdbf_raised(rows)) return rows;
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(rows); i++) {
        json_t *row = json_get(rows, i);
        json_t *id = kdbf_row_col(row, "id");
        if (kdbf_raised(id)) { json_free(rows); json_free(out); return id; }
        json_t *st = kdbf_row_col(row, "status");
        if (kdbf_raised(st)) { json_free(id); json_free(rows); json_free(out); return st; }
        json_t *pair = json_array();
        json_append(pair, id);
        json_append(pair, st);
        json_append(out, pair);
    }
    json_free(rows);
    return out;
}

/* PoP: kdbf_missing_task_ids @ hermes_cli/kanban_db.py:_missing_task_ids */
json_t *kdbf_missing_task_ids(kdbf_io_t *io, const json_t *ids) {
    json_t *out = json_array();
    size_t n = (ids && ids->type == JSON_ARRAY) ? json_len(ids) : 0;
    if (n == 0) return out;
    kdbf_sb_t ph;
    kdbf_sb_init(&ph);
    for (size_t i = 0; i < n; i++) {
        if (i) kdbf_sb_add(&ph, ",");
        kdbf_sb_add(&ph, "?");
    }
    kdbf_sb_t sql;
    kdbf_sb_init(&sql);
    kdbf_sb_add(&sql, KDBF_SQL_MISSING_PRE);
    kdbf_sb_add(&sql, ph.p ? ph.p : "");
    kdbf_sb_add(&sql, KDBF_SQL_MISSING_POST);
    kdbf_sb_free(&ph);
    kdbf_cursor_t cur;
    json_t *err = kdbf_execute(io, sql.p ? sql.p : "", json_copy(ids), &cur);
    kdbf_sb_free(&sql);
    if (err) { json_free(out); return err; }
    json_t *rows = kdbf_fetchall(io, &cur);
    kdbf_cursor_clear(&cur);
    if (kdbf_raised(rows)) { json_free(out); return rows; }
    for (size_t i = 0; i < n; i++) {
        json_t *p = json_get(ids, i);
        const char *ps = (p && p->type == JSON_STRING) ? p->str_val : NULL;
        bool present = false;
        if (ps) {
            for (size_t j = 0; j < json_len(rows); j++) {
                json_t *r = json_get(rows, j);
                json_t *idv = (r && r->type == JSON_OBJECT) ? json_obj_get(r, "id") : NULL;
                if (idv && idv->type == JSON_STRING && strcmp(idv->str_val, ps) == 0) {
                    present = true;
                    break;
                }
            }
        }
        if (!present) json_append(out, json_copy(p));
    }
    json_free(rows);
    return out;
}

/* PoP: kdbf_prior_reviewer @ hermes_cli/kanban_db.py:_prior_reviewer */
json_t *kdbf_prior_reviewer(kdbf_io_t *io, const char *task_id) {
    kdbf_cursor_t cur;
    json_t *params = json_array();
    json_append(params, json_string(task_id));
    json_t *err = kdbf_execute(io, KDBF_SQL_PRIOR_RUN, params, &cur);
    if (err) return err;
    json_t *changes_run = kdbf_fetchone(io, &cur);
    kdbf_cursor_clear(&cur);
    if (kdbf_raised(changes_run)) return changes_run;
    if (kdbf_is_none(changes_run)) {
        json_free(changes_run);
        return json_null();
    }
    json_t *run_id = kdbf_row_col(changes_run, "id");
    json_free(changes_run);
    if (kdbf_raised(run_id)) return run_id;
    json_t *ev = kdbf_latest_event(io, task_id, "changes_requested", run_id);
    json_free(run_id);
    if (kdbf_raised(ev)) return ev;
    json_t *payload = kdbf_row_get(ev, "payload", NULL);
    json_free(ev);
    json_t *d = kdbf_json_dict(payload);
    json_free(payload);
    json_t *reviewer = json_obj_get(d, "reviewer");
    json_t *ret;
    if (reviewer && reviewer->type == JSON_STRING) {
        char *s = kdbf_strip(reviewer->str_val ? reviewer->str_val : "");
        ret = (s && s[0] != '\0') ? json_copy(reviewer) : json_bool(false);
        free(s);
    } else {
        ret = json_bool(false);
    }
    json_free(d);
    return ret;
}

/* PoP: kdbf_claim_is_live @ hermes_cli/kanban_db.py:_claim_is_live */
json_t *kdbf_claim_is_live(kdbf_io_t *io, const json_t *trow) {
    json_t *status = kdbf_row_col(trow, "status");
    if (kdbf_raised(status)) return status;
    bool running = status->type == JSON_STRING && strcmp(status->str_val, "running") == 0;
    json_free(status);
    if (!running) return json_bool(false);
    json_t *lock = kdbf_row_col(trow, "claim_lock");
    if (kdbf_raised(lock)) return lock;
    if (kdbf_is_none(lock)) {
        json_free(lock);
        return json_bool(false);
    }
    json_free(lock);
    json_t *pid = kdbf_row_col(trow, "worker_pid");
    if (kdbf_raised(pid)) return pid;
    if (!kdbf_truthy(pid)) {
        json_free(pid);
        return json_bool(false);
    }
    json_t *started = kdbf_row_col(trow, "worker_started_at");
    if (kdbf_raised(started)) {
        json_free(pid);
        return started;
    }
    json_t *args = json_object();
    json_set(args, "pid", pid);
    json_set(args, "started_at", started);
    json_t *alive = kdbf_seam(io, "_worker_alive", args);
    if (kdbf_raised(alive)) return alive;
    json_t *ret = json_bool(kdbf_truthy(alive));
    json_free(alive);
    return ret;
}

/* ── pure validation / payload leaves ────────────────────────────────────── */

/* PoP: kdbf_validate_model_override @ hermes_cli/kanban_db.py:_validate_model_override */
json_t *kdbf_validate_model_override(const json_t *model, const json_t *provider) {
    const char *mraw = (model && model->type == JSON_STRING && model->str_val) ? model->str_val : "";
    const char *praw = (provider && provider->type == JSON_STRING && provider->str_val) ? provider->str_val : "";
    char *m = kdbf_strip(mraw);
    char *p = kdbf_strip(praw);
    bool has_m = m && m[0] != '\0';
    bool has_p = p && p[0] != '\0';
    if (has_p && !has_m) {
        free(m);
        free(p);
        return kdbf_raise("ValueError", "provider_override requires a model_override");
    }
    json_t *out = json_array();
    json_append(out, has_m ? json_string(m) : json_null());
    json_append(out, has_p ? json_string(p) : json_null());
    free(m);
    free(p);
    return out;
}

/* PoP: kdbf_normalize_task_skills @ hermes_cli/kanban_db.py:_normalize_task_skills */
json_t *kdbf_normalize_task_skills(kdbf_io_t *io, const json_t *skills) {
    if (kdbf_is_none(skills)) return json_null();
    json_t *names = kdbf_seam(io, "get_toolset_names", json_object());
    if (kdbf_raised(names)) return names;
    /* KNOWN_TOOLSET_NAMES = frozenset(name.casefold() for name in ...) */
    json_t *folded = json_array();
    if (names->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(names); i++) {
            json_t *nm = json_get(names, i);
            char *s = kdbf_py_str_node(nm);
            char *f = kdbf_casefold(s ? s : "");
            if (f) json_append(folded, json_string(f));
            free(f);
            free(s);
        }
    }
    json_free(names);
    json_t *cleaned = json_array();
    json_t *seen = json_array();
    json_t *typos = json_array();
    json_t *ret = NULL;
    if (skills->type != JSON_ARRAY) {
        /* iterating a non-iterable would TypeError; fixtures pass lists/None */
        ret = kdbf_raise("TypeError", "'object' is not iterable");
        goto done;
    }
    for (size_t i = 0; i < json_len(skills); i++) {
        json_t *s = json_get(skills, i);
        if (!kdbf_truthy(s)) continue;
        char *raw = kdbf_py_str_node(s);
        char *name = kdbf_strip(raw ? raw : "");
        free(raw);
        if (!name || name[0] == '\0') { free(name); continue; }
        if (strchr(name, ',')) {
            char *rep = kdbf_py_repr_str(name);
            char msg[1024];
            snprintf(msg, sizeof msg,
                "skill name cannot contain comma: %s "
                "(pass a list of separate names instead of a comma-joined string)", rep);
            free(rep);
            free(name);
            ret = kdbf_raise("ValueError", msg);
            goto done;
        }
        char *fold = kdbf_casefold(name);
        bool is_toolset = false;
        for (size_t j = 0; j < json_len(folded); j++) {
            json_t *f = json_get(folded, j);
            if (f && f->type == JSON_STRING && strcmp(f->str_val, fold ? fold : "") == 0) {
                is_toolset = true;
                break;
            }
        }
        free(fold);
        if (is_toolset) {
            json_append(typos, json_string(name));
            free(name);
            continue;
        }
        bool dup = false;
        for (size_t j = 0; j < json_len(seen); j++) {
            json_t *f = json_get(seen, j);
            if (f && f->type == JSON_STRING && strcmp(f->str_val, name) == 0) {
                dup = true;
                break;
            }
        }
        if (dup) { free(name); continue; }
        json_append(seen, json_string(name));
        json_append(cleaned, json_string(name));
        free(name);
    }
    if (json_len(typos) > 0) {
        kdbf_sb_t quoted;
        kdbf_sb_init(&quoted);
        for (size_t j = 0; j < json_len(typos); j++) {
            if (j) kdbf_sb_add(&quoted, ", ");
            json_t *t = json_get(typos, j);
            char *rep = kdbf_py_repr_str(t->str_val ? t->str_val : "");
            kdbf_sb_add(&quoted, rep);
            free(rep);
        }
        const char *noun = json_len(typos) == 1 ? "is a toolset name" : "are toolset names";
        char msg[2048];
        snprintf(msg, sizeof msg,
            "%s %s, not skill name(s). "
            "Put toolsets in the assignee profile's `toolsets:` config "
            "instead of per-task skills. Skills are named skill bundles "
            "(e.g. `blogwatcher`, `github-code-review`); toolsets are runtime "
            "capabilities (e.g. `web`, `browser`, `terminal`).",
            quoted.p ? quoted.p : "", noun);
        kdbf_sb_free(&quoted);
        ret = kdbf_raise("ValueError", msg);
        goto done;
    }
    ret = cleaned;
    cleaned = NULL;
done:
    json_free(cleaned);
    json_free(seen);
    json_free(typos);
    json_free(folded);
    return ret;
}

/* PoP: kdbf_completed_event_payload @ hermes_cli/kanban_db.py:_completed_event_payload */
json_t *kdbf_completed_event_payload(const json_t *result, const json_t *event_summary,
                                     const json_t *verified_cards, const json_t *metadata) {
    json_t *payload = json_object();
    double rlen = 0;
    if (kdbf_truthy(result) && result->type == JSON_STRING)
        rlen = (double)kdbf_py_len(result->str_val ? result->str_val : "");
    json_set(payload, "result_len", json_number(rlen));
    char *fl = kdbf_first_line(event_summary, 400);
    json_set(payload, "summary", (fl && fl[0] != '\0') ? json_string(fl) : json_null());
    free(fl);
    if (kdbf_truthy(verified_cards))
        json_set(payload, "verified_cards", json_copy(verified_cards));
    if (metadata && metadata->type == JSON_OBJECT) {
        json_t *cleaned = kdbf_cleaned_artifact_paths(metadata);
        if (json_len(cleaned) > 0)
            json_set(payload, "artifacts", cleaned);
        else
            json_free(cleaned);
    }
    return payload;
}

/* PoP: kdbf_route_block_flow @ hermes_cli/kanban_db.py:_route_block */
json_t *kdbf_route_block_flow(const json_t *kind, const json_t *reason, const char *source_status,
                              const json_t *prev_kind, double prev_recurrences) {
    const char *ks = (kind && kind->type == JSON_STRING) ? kind->str_val : NULL;
    const char *pks = (prev_kind && prev_kind->type == JSON_STRING) ? prev_kind->str_val : NULL;
    json_t *payload = json_object();
    json_set(payload, "reason", kdbf_is_none(reason) ? json_null() : json_copy(reason));
    json_set(payload, "kind", kdbf_is_none(kind) ? json_null() : json_copy(kind));
    json_set(payload, "source_status", json_string(source_status));
    json_t *out = json_array();
    if (ks && strcmp(ks, "dependency") == 0) {
        json_append(out, json_string("todo"));
        json_append(out, json_string("dependency_wait"));
        json_append(out, json_string(KDBF_ROUTE_DEP_SET_SQL));
        json_t *params = json_array();
        json_append(params, json_copy(kind));
        json_append(out, params);
        json_append(out, payload);
        return out;
    }
    bool same_kind = (kdbf_is_none(kind) && kdbf_is_none(prev_kind)) ||
                     (ks && pks && strcmp(ks, pks) == 0);
    double recurrences = same_kind ? prev_recurrences + 1 : 1;
    json_free(payload);
    payload = json_object();
    json_set(payload, "reason", kdbf_is_none(reason) ? json_null() : json_copy(reason));
    json_set(payload, "kind", kdbf_is_none(kind) ? json_null() : json_copy(kind));
    json_set(payload, "recurrences", json_number(recurrences));
    json_set(payload, "source_status", json_string(source_status));
    json_t *params = json_array();
    json_append(params, kdbf_is_none(kind) ? json_null() : json_copy(kind));
    json_append(params, json_number(recurrences));
    if (recurrences >= KDBF_BLOCK_RECURRENCE_LIMIT) {
        json_set(payload, "limit", json_number(KDBF_BLOCK_RECURRENCE_LIMIT));
        json_append(out, json_string("triage"));
        json_append(out, json_string("block_loop_detected"));
    } else {
        json_append(out, json_string("blocked"));
        json_append(out, json_string("blocked"));
    }
    json_append(out, json_string(KDBF_ROUTE_SET_SQL));
    json_append(out, params);
    json_append(out, payload);
    return out;
}

/* ── write-side flows ────────────────────────────────────────────────────── */

/* PoP: kdbf_insert_comment @ hermes_cli/kanban_db.py:_insert_comment */
json_t *kdbf_insert_comment(kdbf_io_t *io, const char *task_id, const char *author,
                            const char *body, double created_at) {
    json_t *params = json_array();
    json_append(params, json_string(task_id));
    json_append(params, json_string(author ? author : ""));
    json_append(params, json_string(body ? body : ""));
    json_append(params, json_number(created_at));
    kdbf_cursor_t cur;
    json_t *err = kdbf_execute(io, KDBF_SQL_INSERT_COMMENT, params, &cur);
    kdbf_cursor_clear(&cur);
    if (err) return err;
    return json_null();
}

/* PoP: kdbf_delete_task_relations_flow @ hermes_cli/kanban_db.py:_delete_task_relations */
json_t *kdbf_delete_task_relations_flow(kdbf_io_t *io, const char *task_id) {
    kdbf_cursor_t cur;
    json_t *params = json_array();
    json_append(params, json_string(task_id));
    json_append(params, json_string(task_id));
    json_t *err = kdbf_execute(io, KDBF_SQL_DELETE_LINKS, params, &cur);
    kdbf_cursor_clear(&cur);
    if (err) return err;
    static const char *tables[] = { "task_comments", "task_events", "task_runs", "kanban_notify_subs" };
    for (size_t i = 0; i < sizeof tables / sizeof tables[0]; i++) {
        kdbf_sb_t sql;
        kdbf_sb_init(&sql);
        kdbf_sb_add(&sql, KDBF_SQL_DELETE_TABLE_PRE);
        kdbf_sb_add(&sql, tables[i]);
        kdbf_sb_add(&sql, KDBF_SQL_DELETE_TABLE_POST);
        json_t *p2 = json_array();
        json_append(p2, json_string(task_id));
        err = kdbf_execute(io, sql.p ? sql.p : "", p2, &cur);
        kdbf_sb_free(&sql);
        kdbf_cursor_clear(&cur);
        if (err) return err;
    }
    return json_null();
}

/* PoP: kdbf_record_reclaim_flow @ hermes_cli/kanban_db.py:_record_reclaim */
json_t *kdbf_record_reclaim_flow(kdbf_io_t *io, const char *task_id, const json_t *termination,
                                 const char *error, const json_t *payload) {
    json_t *args = json_object();
    json_set(args, "task_id", json_string(task_id));
    json_set(args, "outcome", json_string("reclaimed"));
    json_set(args, "status", json_string("reclaimed"));
    json_set(args, "summary", json_null());
    json_set(args, "error", json_string(error ? error : ""));
    json_set(args, "metadata", kdbf_is_none(termination) ? json_null() : json_copy(termination));
    json_t *run_id = kdbf_seam(io, "_end_run", args);
    if (kdbf_raised(run_id)) return run_id;
    /* payload.update(termination) — existing keys keep their position. */
    json_t *merged = kdbf_is_none(payload) ? json_object() : json_copy(payload);
    if (termination && termination->type == JSON_OBJECT) {
        for (size_t i = 0; i < termination->c.count; i++)
            json_set(merged, termination->c.keys[i], json_copy(termination->c.items[i]));
    }
    json_t *ev_args = json_object();
    json_set(ev_args, "task_id", json_string(task_id));
    json_set(ev_args, "kind", json_string("reclaimed"));
    json_set(ev_args, "payload", merged);
    json_set(ev_args, "run_id", json_copy(run_id));
    json_t *ev = kdbf_seam(io, "_append_event", ev_args);
    if (kdbf_raised(ev)) {
        json_free(run_id);
        return ev;
    }
    json_free(ev);
    return run_id;
}

/* PoP: kdbf_set_task_override_flow @ hermes_cli/kanban_db.py:_set_task_override */
json_t *kdbf_set_task_override_flow(kdbf_io_t *io, const char *task_id, const char *sql,
                                    const json_t *params, const char *event_kind,
                                    const json_t *payload, const json_t *changed_fields,
                                    const char *archived_msg) {
    json_t *tx = kdbf_txn_enter(io, false);
    if (kdbf_raised(tx)) return tx;
    json_t *ret = NULL;
    json_t *status = kdbf_task_status(io, task_id);
    if (kdbf_raised(status)) { ret = status; status = NULL; goto out; }
    if (kdbf_is_none(status)) {
        ret = json_bool(false);
        goto out;
    }
    if (status->type == JSON_STRING && strcmp(status->str_val, "archived") == 0) {
        char msg[512];
        snprintf(msg, sizeof msg, "%s on archived task %s", archived_msg ? archived_msg : "", task_id);
        ret = kdbf_raise("RuntimeError", msg);
        goto out;
    }
    {
        json_t *exec_params = kdbf_is_none(params) ? json_array() : json_copy(params);
        json_append(exec_params, json_string(task_id));
        kdbf_cursor_t cur;
        json_t *err = kdbf_execute(io, sql, exec_params, &cur);
        kdbf_cursor_clear(&cur);
        if (err) { ret = err; goto out; }
    }
    {
        json_t *ev_args = json_object();
        json_set(ev_args, "task_id", json_string(task_id));
        json_set(ev_args, "kind", json_string(event_kind ? event_kind : ""));
        json_set(ev_args, "payload", kdbf_is_none(payload) ? json_null() : json_copy(payload));
        json_set(ev_args, "run_id", json_null());
        json_t *ev = kdbf_seam(io, "_append_event", ev_args);
        if (kdbf_raised(ev)) { ret = ev; goto out; }
        json_free(ev);
    }
    ret = json_bool(true);
out:
    json_free(status);
    {
        json_t *fin = kdbf_txn_finish(io, tx, ret);
        if (kdbf_raised(fin)) return fin;
        if (fin && fin->type == JSON_BOOL && fin->bool_val) {
            json_t *n_args = json_object();
            json_set(n_args, "task_id", json_string(task_id));
            json_set(n_args, "changed_fields", kdbf_is_none(changed_fields) ? json_array() : json_copy(changed_fields));
            json_set(n_args, "board", json_null());
            json_t *notify = kdbf_seam(io, "notify_task_updated", n_args);
            if (kdbf_raised(notify)) {
                json_free(fin);
                return notify;
            }
            json_free(notify);
        }
        return fin;
    }
}

/* PoP: kdbf_gate_empty_completion_flow @ hermes_cli/kanban_db.py:_gate_empty_completion */
json_t *kdbf_gate_empty_completion_flow(kdbf_io_t *io, const char *task_id,
                                        const json_t *result, const json_t *summary) {
    kdbf_cursor_t cur;
    json_t *params = json_array();
    json_append(params, json_string(task_id));
    json_t *err = kdbf_execute(io, KDBF_SQL_GATE_SELECT, params, &cur);
    if (err) return err;
    json_t *row = kdbf_fetchone(io, &cur);
    kdbf_cursor_clear(&cur);
    if (kdbf_raised(row)) return row;
    if (kdbf_is_none(row)) {
        json_free(row);
        return json_null();
    }
    json_t *status = kdbf_row_col(row, "status");
    if (kdbf_raised(status)) { json_free(row); return status; }
    bool review = status->type == JSON_STRING && strcmp(status->str_val, "review") == 0;
    json_free(status);
    if (review) {
        json_free(row);
        return json_null();
    }
    json_t *stored = kdbf_row_col(row, "result");
    json_free(row);
    if (kdbf_raised(stored)) return stored;
    bool exempt = kdbf_substantive_text(result) || kdbf_substantive_text(summary) ||
                  kdbf_substantive_text(stored);
    json_free(stored);
    if (exempt) return json_null();
    json_t *tx = kdbf_txn_enter(io, false);
    if (kdbf_raised(tx)) return tx;
    json_t *payload = json_object();
    char *rp = kdbf_first_line(result, 200);
    json_set(payload, "result_preview", (rp && rp[0] != '\0') ? json_string(rp) : json_null());
    free(rp);
    char *sp = kdbf_first_line(summary, 200);
    json_set(payload, "summary_preview", (sp && sp[0] != '\0') ? json_string(sp) : json_null());
    free(sp);
    json_t *ev_args = json_object();
    json_set(ev_args, "task_id", json_string(task_id));
    json_set(ev_args, "kind", json_string("completion_blocked_empty_result"));
    json_set(ev_args, "payload", payload);
    json_set(ev_args, "run_id", json_null());
    json_t *ev = kdbf_seam(io, "_append_event", ev_args);
    if (kdbf_raised(ev)) return kdbf_txn_finish(io, tx, ev);
    json_free(ev);
    json_t *fin = kdbf_txn_finish(io, tx, json_null());
    if (kdbf_raised(fin)) return fin;
    json_free(fin);
    char msg[512];
    snprintf(msg, sizeof msg, "completion blocked: %s has no result or summary evidence", task_id);
    return kdbf_raise("EmptyCompletionError", msg);
}

/* PoP: kdbf_extend_live_stale_claim_flow @ hermes_cli/kanban_db.py:_extend_live_stale_claim */
json_t *kdbf_extend_live_stale_claim_flow(kdbf_io_t *io, const json_t *row, double now) {
    json_t *ttl_args = json_object();
    json_set(ttl_args, "ttl_seconds", json_null());
    json_t *ttl = kdbf_seam(io, "_resolve_claim_ttl_seconds", ttl_args);
    if (kdbf_raised(ttl)) return ttl;
    double new_expires = now + (ttl->type == JSON_NUMBER ? ttl->num_val : 0);
    json_free(ttl);
    json_t *tx = kdbf_txn_enter(io, false);
    if (kdbf_raised(tx)) return tx;
    json_t *ret = NULL;
    json_t *id = kdbf_row_col(row, "id");
    if (kdbf_raised(id)) { ret = id; id = NULL; goto out; }
    json_t *lock = kdbf_row_col(row, "claim_lock");
    if (kdbf_raised(lock)) { ret = lock; lock = NULL; goto out; }
    {
        json_t *params = json_array();
        json_append(params, json_number(new_expires));
        json_append(params, json_copy(id));
        json_append(params, json_copy(lock));
        json_append(params, json_number(now));
        kdbf_cursor_t cur;
        json_t *err = kdbf_execute(io, KDBF_SQL_STALE_CAS, params, &cur);
        bool won = !err && kdbf_rowcount(&cur) == 1;
        kdbf_cursor_clear(&cur);
        if (err) { ret = err; goto out; }
        if (!won) { ret = json_null(); goto out; }
    }
    {
        const char *ids = (id->type == JSON_STRING) ? id->str_val : "";
        json_t *rid = kdbf_extend_run_claim(io, ids, new_expires);
        if (kdbf_raised(rid)) { ret = rid; goto out; }
        json_t *pid = kdbf_row_col(row, "worker_pid");
        if (kdbf_raised(pid)) { json_free(rid); ret = pid; goto out; }
        json_t *pid_int = kdbf_py_int_node(pid);
        json_free(pid);
        if (kdbf_raised(pid_int)) { json_free(rid); ret = pid_int; goto out; }
        json_t *ce = kdbf_row_col(row, "claim_expires");
        if (kdbf_raised(ce)) { json_free(rid); json_free(pid_int); ret = ce; goto out; }
        json_t *ce_args = json_object();
        json_set(ce_args, "val", ce);
        json_t *ce_epoch = kdbf_seam(io, "_to_epoch", ce_args);
        if (kdbf_raised(ce_epoch)) { json_free(rid); json_free(pid_int); ret = ce_epoch; goto out; }
        json_t *lh = kdbf_row_col(row, "last_heartbeat_at");
        if (kdbf_raised(lh)) { json_free(rid); json_free(pid_int); json_free(ce_epoch); ret = lh; goto out; }
        json_t *lh_args = json_object();
        json_set(lh_args, "val", lh);
        json_t *lh_epoch = kdbf_seam(io, "_to_epoch", lh_args);
        if (kdbf_raised(lh_epoch)) { json_free(rid); json_free(pid_int); json_free(ce_epoch); ret = lh_epoch; goto out; }
        json_t *payload = json_object();
        json_set(payload, "reason", json_string("pid_alive"));
        json_set(payload, "worker_pid", pid_int);
        json_set(payload, "claim_lock", json_copy(lock));
        json_set(payload, "claim_expires_was", ce_epoch);
        json_set(payload, "claim_expires_now", json_number(new_expires));
        json_set(payload, "last_heartbeat_at", lh_epoch);
        json_t *ev_args = json_object();
        json_set(ev_args, "task_id", json_copy(id));
        json_set(ev_args, "kind", json_string("claim_extended"));
        json_set(ev_args, "payload", payload);
        json_set(ev_args, "run_id", rid);
        json_t *ev = kdbf_seam(io, "_append_event", ev_args);
        if (kdbf_raised(ev)) { ret = ev; goto out; }
        json_free(ev);
    }
    ret = json_null();
out:
    json_free(id);
    json_free(lock);
    return kdbf_txn_finish(io, tx, ret);
}

/* PoP: kdbf_claim_and_open_run_flow @ hermes_cli/kanban_db.py:_claim_and_open_run */
json_t *kdbf_claim_and_open_run_flow(kdbf_io_t *io, const char *task_id, const char *source_status,
                                     const char *lock, double expires, double now,
                                     const json_t *event_extra, const json_t *expect_assignee) {
    kdbf_sb_t sql;
    kdbf_sb_init(&sql);
    kdbf_sb_add(&sql, KDBF_SQL_CLAIM_CAS_PRE);
    kdbf_sb_add(&sql, source_status ? source_status : "");
    kdbf_sb_add(&sql, KDBF_SQL_CLAIM_CAS_POST);
    json_t *params = json_array();
    json_append(params, json_string(lock ? lock : ""));
    json_append(params, json_number(expires));
    json_append(params, json_number(now));
    json_append(params, json_string(task_id));
    json_append(params, kdbf_is_none(expect_assignee) ? json_null() : json_copy(expect_assignee));
    json_append(params, kdbf_is_none(expect_assignee) ? json_null() : json_copy(expect_assignee));
    kdbf_cursor_t cur;
    json_t *err = kdbf_execute(io, sql.p ? sql.p : "", params, &cur);
    kdbf_sb_free(&sql);
    bool won = !err && kdbf_rowcount(&cur) == 1;
    kdbf_cursor_clear(&cur);
    if (err) return err;
    if (!won) return json_null();
    json_t *p2 = json_array();
    json_append(p2, json_string(task_id));
    err = kdbf_execute(io, KDBF_SQL_CLAIM_TROW, p2, &cur);
    if (err) return err;
    json_t *trow = kdbf_fetchone(io, &cur);
    kdbf_cursor_clear(&cur);
    if (kdbf_raised(trow)) return trow;
    json_t *assignee = json_null();
    json_t *step = json_null();
    json_t *maxrt = json_null();
    if (kdbf_truthy(trow)) {
        assignee = kdbf_row_col(trow, "assignee");
        if (kdbf_raised(assignee)) { json_free(trow); return assignee; }
        step = kdbf_row_col(trow, "current_step_key");
        if (kdbf_raised(step)) { json_free(trow); json_free(assignee); return step; }
        maxrt = kdbf_row_col(trow, "max_runtime_seconds");
        if (kdbf_raised(maxrt)) { json_free(trow); json_free(assignee); json_free(step); return maxrt; }
    }
    json_free(trow);
    json_t *ins = json_array();
    json_append(ins, json_string(task_id));
    json_append(ins, assignee);
    json_append(ins, step);
    json_append(ins, json_string(lock ? lock : ""));
    json_append(ins, json_number(expires));
    json_append(ins, maxrt);
    json_append(ins, json_number(now));
    err = kdbf_execute(io, KDBF_SQL_CLAIM_INSERT, ins, &cur);
    if (err) return err;
    json_t *run_id = kdbf_lastrowid(&cur);
    kdbf_cursor_clear(&cur);
    json_t *p3 = json_array();
    json_append(p3, json_copy(run_id));
    json_append(p3, json_string(task_id));
    err = kdbf_execute(io, KDBF_SQL_CLAIM_SETCURRENT, p3, &cur);
    kdbf_cursor_clear(&cur);
    if (err) { json_free(run_id); return err; }
    json_t *payload = json_object();
    json_set(payload, "lock", json_string(lock ? lock : ""));
    json_set(payload, "expires", json_number(expires));
    json_set(payload, "run_id", json_copy(run_id));
    if (event_extra && event_extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < event_extra->c.count; i++)
            json_set(payload, event_extra->c.keys[i], json_copy(event_extra->c.items[i]));
    }
    json_t *ev_args = json_object();
    json_set(ev_args, "task_id", json_string(task_id));
    json_set(ev_args, "kind", json_string("claimed"));
    json_set(ev_args, "payload", payload);
    json_set(ev_args, "run_id", json_copy(run_id));
    json_t *ev = kdbf_seam(io, "_append_event", ev_args);
    if (kdbf_raised(ev)) { json_free(run_id); return ev; }
    json_free(ev);
    return run_id;
}

/* PoP: kdbf_edit_task_flow @ hermes_cli/kanban_db.py:edit_task */
json_t *kdbf_edit_task_flow(kdbf_io_t *io, const char *task_id, const json_t *title,
                            const json_t *body, const json_t *priority, const json_t *result,
                            const json_t *summary, const json_t *metadata, const json_t *board) {
    const char *fields[3] = { "title", "body", "priority" };
    const json_t *vals[3] = { title, body, priority };
    json_t *changed = json_array();
    for (int i = 0; i < 3; i++)
        if (!kdbf_is_none(vals[i])) json_append(changed, json_string(fields[i]));
    json_t *tx = kdbf_txn_enter(io, false);
    if (kdbf_raised(tx)) { json_free(changed); return tx; }
    json_t *ret = NULL;
    json_t *status = NULL;
    json_t *handoff = NULL;
    json_t *run_id = NULL;
    json_t *run = NULL;
    status = kdbf_task_status(io, task_id);
    if (kdbf_raised(status)) { ret = status; status = NULL; goto out; }
    bool done = status->type == JSON_STRING && strcmp(status->str_val, "done") == 0;
    if (kdbf_is_none(status) || (!kdbf_is_none(result) && !done)) {
        ret = json_bool(false);
        goto out;
    }
    {
        kdbf_sb_t asg;
        kdbf_sb_init(&asg);
        json_t *params = json_array();
        for (int i = 0; i < 3; i++) {
            if (kdbf_is_none(vals[i])) continue;
            if (asg.len) kdbf_sb_add(&asg, ", ");
            kdbf_sb_add(&asg, fields[i]);
            kdbf_sb_add(&asg, " = ?");
            json_append(params, json_copy(vals[i]));
        }
        if (!kdbf_is_none(result)) {
            if (asg.len) kdbf_sb_add(&asg, ", ");
            kdbf_sb_add(&asg, "result = ?");
            json_append(params, json_copy(result));
            json_append(changed, json_string("result"));
        }
        if (json_len(params) == 0) {
            kdbf_sb_free(&asg);
            json_free(params);
            ret = json_bool(false);
            goto out;
        }
        kdbf_sb_t sql;
        kdbf_sb_init(&sql);
        kdbf_sb_add(&sql, KDBF_SQL_EDIT_UPDATE_PRE);
        kdbf_sb_add(&sql, asg.p ? asg.p : "");
        kdbf_sb_add(&sql, KDBF_SQL_EDIT_UPDATE_POST);
        kdbf_sb_free(&asg);
        json_append(params, json_string(task_id));
        kdbf_cursor_t cur;
        json_t *err = kdbf_execute(io, sql.p ? sql.p : "", params, &cur);
        kdbf_sb_free(&sql);
        kdbf_cursor_clear(&cur);
        if (err) { ret = err; goto out; }
    }
    if (!kdbf_is_none(priority)) {
        json_t *pl = json_object();
        json_set(pl, "priority", json_copy(priority));
        json_t *ev_args = json_object();
        json_set(ev_args, "task_id", json_string(task_id));
        json_set(ev_args, "kind", json_string("reprioritized"));
        json_set(ev_args, "payload", pl);
        json_set(ev_args, "run_id", json_null());
        json_t *ev = kdbf_seam(io, "_append_event", ev_args);
        if (kdbf_raised(ev)) { ret = ev; goto out; }
        json_free(ev);
    }
    if (kdbf_is_none(result)) {
        json_t *nonprio = json_array();
        for (size_t i = 0; i < json_len(changed); i++) {
            json_t *f = json_get(changed, i);
            if (f && f->type == JSON_STRING && strcmp(f->str_val, "priority") != 0)
                json_append(nonprio, json_copy(f));
        }
        if (json_len(nonprio) > 0) {
            json_t *pl = json_object();
            json_set(pl, "fields", nonprio);
            json_t *ev_args = json_object();
            json_set(ev_args, "task_id", json_string(task_id));
            json_set(ev_args, "kind", json_string("edited"));
            json_set(ev_args, "payload", pl);
            json_set(ev_args, "run_id", json_null());
            json_t *ev = kdbf_seam(io, "_append_event", ev_args);
            if (kdbf_raised(ev)) { ret = ev; goto out; }
            json_free(ev);
        } else {
            json_free(nonprio);
        }
    } else {
        handoff = !kdbf_is_none(summary) ? json_copy(summary) : json_copy(result);
        json_append(changed, json_string("summary"));
        if (!kdbf_is_none(metadata)) json_append(changed, json_string("metadata"));
        json_t *p2 = json_array();
        json_append(p2, json_string(task_id));
        kdbf_cursor_t cur;
        json_t *err = kdbf_execute(io, KDBF_SQL_EDIT_RUN_SELECT, p2, &cur);
        if (err) { ret = err; goto out; }
        run = kdbf_fetchone(io, &cur);
        kdbf_cursor_clear(&cur);
        if (kdbf_raised(run)) { ret = run; run = NULL; goto out; }
        if (kdbf_is_none(run)) {
            json_t *sy_args = json_object();
            json_set(sy_args, "task_id", json_string(task_id));
            json_set(sy_args, "outcome", json_string("completed"));
            json_set(sy_args, "summary", json_copy(handoff));
            json_set(sy_args, "error", json_null());
            json_set(sy_args, "metadata", kdbf_is_none(metadata) ? json_null() : json_copy(metadata));
            json_set(sy_args, "profile", json_null());
            run_id = kdbf_seam(io, "_synthesize_ended_run", sy_args);
            if (kdbf_raised(run_id)) { ret = run_id; run_id = NULL; goto out; }
        } else {
            json_t *idv = kdbf_row_col(run, "id");
            if (kdbf_raised(idv)) { ret = idv; goto out; }
            run_id = kdbf_py_int_node(idv);
            json_free(idv);
            if (kdbf_raised(run_id)) { ret = run_id; run_id = NULL; goto out; }
            json_t *p3 = json_array();
            json_append(p3, json_copy(handoff));
            json_append(p3, json_copy(run_id));
            err = kdbf_execute(io, KDBF_SQL_EDIT_RUN_SUMMARY, p3, &cur);
            kdbf_cursor_clear(&cur);
            if (err) { ret = err; goto out; }
            if (!kdbf_is_none(metadata)) {
                char *dumps = kdbf_py_dumps(metadata);
                json_t *p4 = json_array();
                json_append(p4, json_string(dumps ? dumps : "null"));
                free(dumps);
                json_append(p4, json_copy(run_id));
                err = kdbf_execute(io, KDBF_SQL_EDIT_RUN_METADATA, p4, &cur);
                kdbf_cursor_clear(&cur);
                if (err) { ret = err; goto out; }
            }
        }
        json_free(run);
        run = NULL;
        json_t *flist = json_array();
        json_append(flist, json_string("result"));
        json_append(flist, json_string("summary"));
        if (!kdbf_is_none(metadata)) json_append(flist, json_string("metadata"));
        double rlen = 0;
        if (kdbf_truthy(result) && result->type == JSON_STRING)
            rlen = (double)kdbf_py_len(result->str_val ? result->str_val : "");
        char *fl = kdbf_first_line(handoff, 400);
        json_t *pl = json_object();
        json_set(pl, "fields", flist);
        json_set(pl, "result_len", json_number(rlen));
        json_set(pl, "summary", (fl && fl[0] != '\0') ? json_string(fl) : json_null());
        free(fl);
        json_t *ev_args = json_object();
        json_set(ev_args, "task_id", json_string(task_id));
        json_set(ev_args, "kind", json_string("edited"));
        json_set(ev_args, "payload", pl);
        json_set(ev_args, "run_id", json_copy(run_id));
        json_t *ev = kdbf_seam(io, "_append_event", ev_args);
        if (kdbf_raised(ev)) { ret = ev; goto out; }
        json_free(ev);
    }
    ret = json_bool(true);
out:
    json_free(status);
    json_free(handoff);
    json_free(run_id);
    json_free(run);
    {
        json_t *fin = kdbf_txn_finish(io, tx, ret);
        if (kdbf_raised(fin)) { json_free(changed); return fin; }
        if (fin && fin->type == JSON_BOOL && fin->bool_val) {
            json_t *n_args = json_object();
            json_set(n_args, "task_id", json_string(task_id));
            json_set(n_args, "changed_fields", changed);
            json_set(n_args, "board", kdbf_is_none(board) ? json_null() : json_copy(board));
            json_t *notify = kdbf_seam(io, "notify_task_updated", n_args);
            if (kdbf_raised(notify)) {
                json_free(fin);
                return notify;
            }
            json_free(notify);
        } else {
            json_free(changed);
        }
        return fin;
    }
}
