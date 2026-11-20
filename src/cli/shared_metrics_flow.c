/* shared_metrics_flow.c — FLOW + leaf port of the UNCLAIMED remainder of
 * hermes_cli/observability/shared_metrics.py (the DISTINCT SharedMetricsStore
 * module; the relay_shared_metrics.py siblings live in relay_shared_metrics*.c
 * and are NOT this module). The claimed siblings (__init__, _connection,
 * _utc_now, _isoformat, _parse_state_timestamp, _validate_counter,
 * _install_id, _record_counter_in_transaction, _package_metric, record_counter,
 * record_model_call, record_client_active, _ensure_schema, _create_package*,
 * _export*, _prune_expired_history, counter_snapshot, create_and_export_package*)
 * are SEAMED here through the injected universal seam shm_io_t.
 *
 * Ported (21 unclaimed): drain_deferring_stores, _compact_json, _row_resource,
 * _ensure_private, record_install_snapshot, install_snapshot_due,
 * _record_once_per_interval, update_rollup_state, _update_rollup_state,
 * recorded_milestones, record_milestone, recorded_features,
 * record_feature_adoption, record_counter_once_per_day, _latched,
 * _record_once_ever, _write_or_defer, drain_deferred, _write, _mark_packaged,
 * _package_metrics. Ported functions call each other REAL (internal
 * composition: record_install_snapshot -> _record_once_per_interval,
 * record_milestone/record_feature_adoption -> _record_once_ever,
 * recorded_milestones/recorded_features -> _latched, update_rollup_state ->
 * _write_or_defer -> _update_rollup_state, the latch cores -> _write).
 *
 * Representation contract (shared with tests/oracle_shared_metrics_flow.py):
 *   - raise specs are VALUES {"__raise__": kind, "message": msg} threaded through
 *     every helper; an uncaught one becomes the case ret. The BaseException trio
 *     (SystemExit / KeyboardInterrupt / GeneratorExit) plus asyncio.CancelledError
 *     passes through every `except Exception` / `except OSError` guard exactly as
 *     live. `except sqlite3.OperationalError` matches kind == "OperationalError".
 *   - fail-closed: an absent / exhausted seam script returns a RuntimeError raise
 *     spec ("no script for <name>"), recorded as the event ret BEFORE propagation.
 *   - datetimes (_utc_now / _parse_state_timestamp seam results) travel as
 *     canonical ISO-8601 strings "YYYY-MM-DDTHH:MM:SS[.ffffff](Z|+HH:MM|-HH:MM)";
 *     C parses them to epoch-microseconds for the > / < / +24h comparisons.
 *     now.date().isoformat() == the first 10 chars. Domain: canonical
 *     datetime.isoformat() output (seconds present; fraction 6 digits or absent;
 *     no date-only, no null _utc_now except where .date() is the first access).
 *   - sqlite I/O rides ONE seam "db.execute" {sql, params} whose script result is
 *     a CURSOR SPEC {"one": row|null, "all": [rows], "rowcount": N} (defaults
 *     one=null, all=[], rowcount=0). Rows are attribute dicts read by subscript;
 *     a missing column raises IndexError("No item with that key") (sqlite3.Row).
 *     The connection handle is the opaque "store._connection" seam result; it is
 *     carried as the "connection" arg of write_txn / _install_id /
 *     _record_counter_in_transaction (dropped when null on both sides).
 *   - deferred writes are callable doubles: {"__callable__": "<seam>"} fires that
 *     seam with {"args":[connection],"kwargs":{}}; {"__fn__":"_update_rollup_state",
 *     "state_key":..,"update":..} is the REAL internal rollup lambda. _deferred /
 *     _drain_at_exit live in the state carrier; _DEFERRING_STORES is a list of
 *     {"id":..} store markers (append self == append {"id": state.id}).
 *   - scripts: a bare JSON array is a per-call QUEUE; a list RESULT must be
 *     wrapped {"__script__":true,"ret":[...]}. A scripted null returns None.
 *     None-valued seam-arg OBJECT keys dropped on both sides. Seam results are
 *     fresh copies per call (no cross-call object identity).
 *   - _compact_json domain: str/int/bool/null/list/dict (NO floats — libjson has
 *     no int/float identity, so json.dumps float repr would diverge). Keys sorted
 *     (ASCII strcmp == codepoint order), separators (",",":").
 *   - integral floats collapse to ints on both sides; ASCII scope throughout.
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "cli/shared_metrics_flow.c". Prefix shm_ (sm_ / shared_metrics_ /
 * metrics_ are taken by the claimed-sibling port src/hermes_cli/shared_metrics.c).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

/* ── constants (byte-exact from the quarry module / contract) ── */
#define SM_BUSY_TIMEOUT_MS        250
#define SM_SCHEMA_BUSY_TIMEOUT_MS 5000
#define SM_DEFERRED_WRITES_MAX    1024
#define SM_INSTALL_SNAPSHOT_KEY   "install_snapshot_recorded_at"
#define SM_MILESTONE_PREFIX       "milestone:"
#define SM_FEATURE_PREFIX         "feature:"
#define SM_DAILY_ONCE_PREFIX      "daily_once:"
#define SM_INSTALL_SNAPSHOT_METRIC "hermes.install.snapshot"
#define SM_MILESTONE_METRIC        "hermes.install.milestone"
#define SM_FEATURE_METRIC          "hermes.feature_adoption.count"
#define SM_INTERVAL_US            86400000000LL   /* timedelta(hours=24) in us */

static const char *SM_RESOURCE_COLUMNS[4] = {
    "hermes_version", "os_family", "architecture", "install_method"
};

#define SQL_SELECT_VALUE \
    "SELECT value FROM telemetry_state WHERE key = ?"
#define SQL_STATE_UPSERT \
    "INSERT INTO telemetry_state(key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value"
#define SQL_UPDATE_STATE \
    "UPDATE telemetry_state SET value = ? WHERE key = ?"
#define SQL_DAILY_UPSERT \
    "INSERT INTO telemetry_state(key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value WHERE telemetry_state.value != excluded.value"
#define SQL_SELECT_KEY_LIKE \
    "SELECT key FROM telemetry_state WHERE key LIKE ?"
#define SQL_INSERT_NOTHING \
    "INSERT INTO telemetry_state(key, value) VALUES (?, ?) ON CONFLICT(key) DO NOTHING"
#define SQL_UPDATE_PACKAGED \
    "\n                    UPDATE counter_aggregates SET packaged_value = value\n                    WHERE period_start = ? AND metric_name = ? AND hermes_version = ?\n                      AND os_family = ? AND architecture = ? AND install_method = ?\n                      AND dimensions_json = ?\n                    "

/* ── universal seam ── */
typedef struct shm_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} shm_io_t;

/* ── truthiness / accessors (Python-faithful) ── */
static bool shm_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool shm_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool shm_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool shm_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool shm_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool shm_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool shm_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return false;
}

static const char *shm_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *shm_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double shm_num(const json_t *o, const char *k, double def)
{
    const json_t *v = shm_get(o, k);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static bool shm_bool(const json_t *o, const char *k, bool def)
{
    const json_t *v = shm_get(o, k);
    return (v && v->type == JSON_BOOL) ? v->bool_val : def;
}

static bool shm_str_eq(const char *a, const char *b)
{
    return strcmp(a ? a : "", b ? b : "") == 0;
}

static json_t *shm_copy_or_null(const json_t *v) { return v ? json_copy(v) : NULL; }

/* Python type name for a JSON value (integral numbers read as int). */
static const char *shm_type_name(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        return (d == (double)(long long)d && fabs(d) < 9.2e18) ? "int" : "float";
    }
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    if (v->type == JSON_OBJECT) return "dict";
    return "object";
}

/* ── raise specs ── */
static bool shm_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *shm_kind(const json_t *r)
{
    const char *k = shm_str(shm_get(r, "__raise__"));
    return k ? k : "";
}
static bool shm_is_base(const char *kind)
{
    return shm_str_eq(kind, "SystemExit") || shm_str_eq(kind, "KeyboardInterrupt")
        || shm_str_eq(kind, "GeneratorExit") || shm_str_eq(kind, "CancelledError");
}
/* FULL builtin OSError subtree (except OSError catches all of these). */
static bool shm_is_oserror(const char *kind)
{
    static const char *tbl[] = {
        "OSError", "IOError", "EnvironmentError", "WindowsError",
        "BlockingIOError", "ChildProcessError", "ConnectionError",
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", "FileExistsError", "FileNotFoundError",
        "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError"
    };
    for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++)
        if (shm_str_eq(kind, tbl[i])) return true;
    return false;
}
static json_t *shm_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}
static char *shm_xfmt(const char *fmt, ...)
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

/* ── universal seam call: args OWNED by caller, freed here; result caller-owned
 * (NULL == None, or a raise spec). Fail-closed when no channel installed. ── */
static json_t *shm_call(shm_io_t *io, const char *name, json_t *args)
{
    if (!io || !io->call) {
        char *m = shm_xfmt("no seam installed for %s", name);
        json_t *r = shm_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* arg builders: None-valued keys dropped on both sides ("" kept). */
static void shm_arg_s(json_t *a, const char *k, const char *v)
{
    if (v) json_set(a, k, json_string(v));
}
static void shm_arg_j(json_t *a, const char *k, const json_t *v)
{
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
static void shm_arg_b(json_t *a, const char *k, bool v)
{
    json_set(a, k, json_bool(v));
}
static void shm_arg_n(json_t *a, const char *k, double v)
{
    json_set(a, k, json_number(v));
}

/* logger seam: {msg, args:[...], exc_info?}. args_arr ownership transferred. */
static json_t *shm_log(shm_io_t *io, const char *level, const char *msg,
                       json_t *args_arr, bool exc_info)
{
    json_t *a = json_object();
    shm_arg_s(a, "msg", msg);
    json_set(a, "args", args_arr ? args_arr : json_array());
    if (exc_info) shm_arg_b(a, "exc_info", true);
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    return shm_call(io, name, a);
}

/* invoke a callable double {"__callable__": seam} with positional/keyword args.
 * args_arr / kwargs_obj ownership transferred (consumed). */
static json_t *shm_invoke_callable(shm_io_t *io, const json_t *cb,
                                   json_t *args_arr, json_t *kwargs_obj)
{
    const char *name = shm_str(shm_get(cb, "__callable__"));
    if (!name) {
        json_free(args_arr);
        json_free(kwargs_obj);
        return shm_mkraise("TypeError", "'NoneType' object is not callable");
    }
    json_t *a = json_object();
    json_set(a, "args", args_arr ? args_arr : json_array());
    json_set(a, "kwargs", kwargs_obj ? kwargs_obj : json_object());
    return shm_call(io, name, a);
}

/* ── _compact_json: json.dumps(value, sort_keys=True, separators=(",",":")) ── */
typedef struct { char *buf; size_t len, cap; } shm_sb_t;
static void shm_sb_grow(shm_sb_t *s, size_t extra)
{
    if (s->len + extra + 1 <= s->cap) return;
    size_t nc = s->cap ? s->cap : 64;
    while (nc < s->len + extra + 1) nc *= 2;
    char *nb = (char *)realloc(s->buf, nc);
    if (!nb) return;
    s->buf = nb; s->cap = nc;
}
static void shm_sb_putc(shm_sb_t *s, char c)
{
    shm_sb_grow(s, 1);
    if (s->buf) { s->buf[s->len++] = c; s->buf[s->len] = '\0'; }
}
static void shm_sb_puts(shm_sb_t *s, const char *p)
{
    size_t n = strlen(p);
    shm_sb_grow(s, n);
    if (s->buf) { memcpy(s->buf + s->len, p, n); s->len += n; s->buf[s->len] = '\0'; }
}
static void shm_ser_string(shm_sb_t *s, const char *p)
{
    shm_sb_putc(s, '"');
    for (; p && *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"') shm_sb_puts(s, "\\\"");
        else if (c == '\\') shm_sb_puts(s, "\\\\");
        else if (c == '\n') shm_sb_puts(s, "\\n");
        else if (c == '\r') shm_sb_puts(s, "\\r");
        else if (c == '\t') shm_sb_puts(s, "\\t");
        else if (c == '\b') shm_sb_puts(s, "\\b");
        else if (c == '\f') shm_sb_puts(s, "\\f");
        else if (c < 0x20) { char t[8]; snprintf(t, sizeof t, "\\u%04x", c); shm_sb_puts(s, t); }
        else shm_sb_putc(s, (char)c);   /* ASCII scope */
    }
    shm_sb_putc(s, '"');
}
static void shm_ser_node(shm_sb_t *s, const json_t *v)
{
    if (!v || v->type == JSON_NULL) { shm_sb_puts(s, "null"); return; }
    if (v->type == JSON_BOOL) { shm_sb_puts(s, v->bool_val ? "true" : "false"); return; }
    if (v->type == JSON_NUMBER) {
        char t[64];
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 9.2e18)
            snprintf(t, sizeof t, "%lld", (long long)d);
        else {
            /* best-effort shortest round-trip (floats excluded from domain) */
            int prec;
            for (prec = 1; prec <= 17; prec++) {
                snprintf(t, sizeof t, "%.*g", prec, d);
                if (strtod(t, NULL) == d) break;
            }
        }
        shm_sb_puts(s, t);
        return;
    }
    if (v->type == JSON_STRING) { shm_ser_string(s, v->str_val); return; }
    if (v->type == JSON_ARRAY) {
        shm_sb_putc(s, '[');
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) shm_sb_putc(s, ',');
            shm_ser_node(s, v->c.items[i]);
        }
        shm_sb_putc(s, ']');
        return;
    }
    if (v->type == JSON_OBJECT) {
        /* sort keys by strcmp (ASCII codepoint order) */
        size_t n = v->c.count;
        size_t *idx = (size_t *)malloc((n ? n : 1) * sizeof(size_t));
        if (!idx) { shm_sb_puts(s, "{}"); return; }
        for (size_t i = 0; i < n; i++) idx[i] = i;
        for (size_t i = 1; i < n; i++) {
            size_t key = idx[i];
            size_t j = i;
            while (j > 0 && strcmp(v->c.keys[idx[j - 1]], v->c.keys[key]) > 0) {
                idx[j] = idx[j - 1]; j--;
            }
            idx[j] = key;
        }
        shm_sb_putc(s, '{');
        for (size_t i = 0; i < n; i++) {
            if (i) shm_sb_putc(s, ',');
            shm_ser_string(s, v->c.keys[idx[i]]);
            shm_sb_putc(s, ':');
            shm_ser_node(s, v->c.items[idx[i]]);
        }
        shm_sb_putc(s, '}');
        free(idx);
        return;
    }
}
/* malloc'd compact JSON string (caller frees). */
static char *shm_compact_json(const json_t *v)
{
    shm_sb_t s = { NULL, 0, 0 };
    shm_ser_node(&s, v);
    if (!s.buf) return strdup("null");
    return s.buf;
}

/* ── datetime: canonical ISO -> epoch microseconds ── */
static bool shm_is_digit(char c) { return c >= '0' && c <= '9'; }
static int shm_digits(const char *s, int n)
{
    int v = 0;
    for (int i = 0; i < n; i++) {
        if (!shm_is_digit(s[i])) return -1;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}
static long long shm_days_from_civil(long long y, int m, int d)
{
    y -= (m <= 2);
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static bool shm_dt_parse(const char *s, long long *out_us)
{
    if (!s) return false;
    size_t len = strlen(s);
    if (len < 19) return false;
    if (s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':')
        return false;
    int Y = shm_digits(s, 4), Mo = shm_digits(s + 5, 2), D = shm_digits(s + 8, 2);
    int H = shm_digits(s + 11, 2), Mi = shm_digits(s + 14, 2), S = shm_digits(s + 17, 2);
    if (Y < 0 || Mo < 1 || Mo > 12 || D < 1 || D > 31 || H < 0 || H > 23 ||
        Mi < 0 || Mi > 59 || S < 0 || S > 59) return false;
    size_t pos = 19;
    long long frac = 0;
    if (pos < len && s[pos] == '.') {
        pos++;
        int fd = 0;
        long long f = 0;
        while (pos < len && shm_is_digit(s[pos]) && fd < 6) {
            f = f * 10 + (s[pos] - '0'); pos++; fd++;
        }
        if (fd == 0) return false;
        while (fd < 6) { f *= 10; fd++; }
        frac = f;
    }
    long long off_us = 0;
    if (pos < len && s[pos] == 'Z') {
        pos++;
    } else if (pos < len && (s[pos] == '+' || s[pos] == '-')) {
        int sign = (s[pos] == '-') ? -1 : 1;
        if (pos + 6 > len || s[pos + 3] != ':') return false;
        int oh = shm_digits(s + pos + 1, 2), om = shm_digits(s + pos + 4, 2);
        if (oh < 0 || om < 0 || oh > 23 || om > 59) return false;
        off_us = (long long)sign * (oh * 60 + om) * 60000000LL;
        pos += 6;
    } else {
        return false;   /* tz required (canonical isoformat always carries it) */
    }
    if (pos != len) return false;
    long long days = shm_days_from_civil(Y, Mo, D);
    long long us = (((days * 24 + H) * 60 + Mi) * 60 + S) * 1000000LL + frac - off_us;
    *out_us = us;
    return true;
}
/* first 10 chars (the local date) of an ISO datetime string. */
static void shm_date_part(const json_t *now, char *out)
{
    const char *s = shm_str(now);
    size_t i = 0;
    if (s) for (; i < 10 && s[i]; i++) out[i] = s[i];
    out[i] = '\0';
}

/* ── sqlite3.Row subscript (missing column -> IndexError) ── */
static json_t *shm_row_get(const json_t *row, const char *key)
{
    if (shm_is_null(row))
        return shm_mkraise("TypeError", "'NoneType' object is not subscriptable");
    if (!shm_is_obj(row)) {
        char *m = shm_xfmt("'%s' object is not subscriptable", shm_type_name(row));
        json_t *e = shm_mkraise("TypeError", m ? m : "not subscriptable");
        free(m);
        return e;
    }
    const json_t *v = json_obj_get(row, key);
    if (!v) return shm_mkraise("IndexError", "No item with that key");
    return json_copy(v);
}
/* list subscript r[i] (out of range -> IndexError; non-list -> TypeError). */
static json_t *shm_index_get(const json_t *r, size_t i)
{
    if (!shm_is_arr(r)) {
        char *m = shm_xfmt("'%s' object is not subscriptable", shm_type_name(r));
        json_t *e = shm_mkraise("TypeError", m ? m : "not subscriptable");
        free(m);
        return e;
    }
    if (i >= r->c.count) return shm_mkraise("IndexError", "list index out of range");
    return json_copy(r->c.items[i]);
}

/* ── db.execute seam + cursor-spec accessors ── */
static json_t *shm_db_execute(shm_io_t *io, const char *sql, json_t *params)
{
    json_t *a = json_object();
    shm_arg_s(a, "sql", sql);
    json_set(a, "params", params ? params : json_array());
    return shm_call(io, "db.execute", a);
}
static json_t *shm_cursor_one(const json_t *spec)
{
    const json_t *v = shm_get(spec, "one");
    return v ? json_copy(v) : NULL;
}
static json_t *shm_cursor_all(const json_t *spec)
{
    const json_t *v = shm_get(spec, "all");
    return (v && v->type == JSON_ARRAY) ? json_copy(v) : json_array();
}
static double shm_cursor_rowcount(const json_t *spec)
{
    return shm_num(spec, "rowcount", 0.0);
}

/* ── _connection seam + ported _write (connection + write_txn seams) ── */
static json_t *shm_connection(shm_io_t *io, double busy_timeout_ms)
{
    json_t *a = json_object();
    shm_arg_n(a, "busy_timeout_ms", busy_timeout_ms);
    return shm_call(io, "store._connection", a);
}
/* ported _write: yields the connection via *conn_out; returns a raise spec or
 * NULL. The context-manager teardown (commit/rollback/close) is silent. */
static json_t *shm_write(shm_io_t *io, json_t *st, double busy_timeout_ms, json_t **conn_out)
{
    (void)st;
    *conn_out = NULL;
    json_t *conn = shm_connection(io, busy_timeout_ms);
    if (shm_is_raise(conn)) return conn;
    json_t *a = json_object();
    shm_arg_j(a, "connection", conn);
    json_t *wt = shm_call(io, "write_txn", a);
    if (shm_is_raise(wt)) { json_free(conn); return wt; }
    json_free(wt);
    *conn_out = conn;
    return NULL;
}

/* forward decls for internal composition */
static json_t *shm_update_rollup_state_core(shm_io_t *io, json_t *st, const json_t *conn,
                                            const char *state_key, const json_t *update);
static json_t *shm_record_once_per_interval_core(shm_io_t *io, json_t *st,
    const char *state_key, const char *metric_name, const json_t *dimensions, const json_t *resource);
static json_t *shm_record_once_ever_core(shm_io_t *io, json_t *st,
    const char *state_key, const char *metric_name, const json_t *dimensions, const json_t *resource);
static json_t *shm_latched_core(shm_io_t *io, json_t *st, const char *prefix);
static json_t *shm_write_or_defer_core(shm_io_t *io, json_t *st, const json_t *write_cb);

/* invoke a deferred write item: __fn__ rollup lambda REAL, else __callable__ seam. */
static json_t *shm_invoke_deferred(shm_io_t *io, json_t *st, const json_t *item, const json_t *conn)
{
    const char *fn = shm_str(shm_get(item, "__fn__"));
    if (fn && strcmp(fn, "_update_rollup_state") == 0) {
        const char *sk = shm_str(shm_get(item, "state_key"));
        const json_t *upd = shm_get(item, "update");
        return shm_update_rollup_state_core(io, st, conn, sk ? sk : "", upd);
    }
    json_t *args = json_array();
    json_append(args, conn ? json_copy(conn) : json_null());
    return shm_invoke_callable(io, item, args, json_object());
}

/* ══════════════════════════════════════════════════════════════════════════
 * 1. _compact_json (module leaf)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_compact_json_func @ hermes_cli/observability/shared_metrics.py:_compact_json */
static json_t *shm_compact_json_func(shm_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    char *s = shm_compact_json(shm_get(in, "value"));
    json_t *r = json_string(s ? s : "null");
    free(s);
    return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 2. _row_resource (module leaf) — {col: row[col] for col in _RESOURCE_COLUMNS}
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_row_resource @ hermes_cli/observability/shared_metrics.py:_row_resource */
static json_t *shm_row_resource(shm_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *row = shm_get(in, "row");
    json_t *out = json_object();
    for (int i = 0; i < 4; i++) {
        json_t *v = shm_row_get(row, SM_RESOURCE_COLUMNS[i]);
        if (shm_is_raise(v)) { json_free(out); return v; }
        json_set(out, SM_RESOURCE_COLUMNS[i], v);
    }
    return out;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 3. _ensure_private (module leaf) — mkdir/touch then chmod (OSError swallowed)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_ensure_private @ hermes_cli/observability/shared_metrics.py:_ensure_private */
static json_t *shm_ensure_private(shm_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *path = shm_str(shm_get(in, "path"));
    double mode = shm_num(in, "mode", 0.0);
    json_t *a = json_object();
    shm_arg_s(a, "path", path);
    if (mode == 448.0) {   /* 0o700 */
        shm_arg_b(a, "parents", true);
        shm_arg_b(a, "exist_ok", true);
        shm_arg_n(a, "mode", mode);
        json_t *r = shm_call(io, "path.mkdir", a);
        if (shm_is_raise(r)) return r;
        json_free(r);
    } else {
        shm_arg_n(a, "mode", mode);
        shm_arg_b(a, "exist_ok", true);
        json_t *r = shm_call(io, "path.touch", a);
        if (shm_is_raise(r)) return r;
        json_free(r);
    }
    json_t *ca = json_object();
    shm_arg_s(ca, "path", path);
    shm_arg_n(ca, "mode", mode);
    json_t *cr = shm_call(io, "path.chmod", ca);
    if (shm_is_raise(cr)) {
        if (shm_is_oserror(shm_kind(cr))) { json_free(cr); return NULL; }
        return cr;   /* BaseException / non-OSError passthrough */
    }
    json_free(cr);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 4. drain_deferring_stores (module) — per-store bounded drain before os._exit
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_drain_deferring_stores @ hermes_cli/observability/shared_metrics.py:drain_deferring_stores */
static json_t *shm_drain_deferring_stores(shm_io_t *io, const json_t *in, json_t *st)
{
    double deadline = shm_num(in, "deadline", 0.0);
    const json_t *stores = shm_get(st, "_DEFERRING_STORES");
    size_t n = shm_is_arr(stores) ? stores->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *mono = shm_call(io, "time.monotonic", json_object());
        if (shm_is_raise(mono)) return mono;
        double m = shm_is_num(mono) ? mono->num_val : 0.0;
        json_free(mono);
        long long remaining = (long long)((deadline - m) * 1000.0);  /* int() truncates */
        if (remaining <= 0) return NULL;
        const json_t *store = stores->c.items[i];
        const char *sid = shm_str(shm_get(store, "id"));
        json_t *a = json_object();
        shm_arg_s(a, "store_id", sid ? sid : "");
        shm_arg_n(a, "busy_timeout_ms", (double)remaining);
        json_t *r = shm_call(io, "store.drain_deferred", a);
        if (shm_is_raise(r)) return r;
        json_free(r);
    }
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 5. _write (contextmanager) — _connection + write_txn; yields connection
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_write_entry @ hermes_cli/observability/shared_metrics.py:_write */
static json_t *shm_write_entry(shm_io_t *io, const json_t *in, json_t *st)
{
    double bt = shm_num(in, "busy_timeout_ms", SM_BUSY_TIMEOUT_MS);
    json_t *conn = NULL;
    json_t *r = shm_write(io, st, bt, &conn);
    if (r) return r;
    json_free(conn);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 6. install_snapshot_due — cheap latch read
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_install_snapshot_due @ hermes_cli/observability/shared_metrics.py:install_snapshot_due */
static json_t *shm_install_snapshot_due(shm_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    json_t *ret = NULL, *conn = NULL, *now = NULL, *last = NULL;
    conn = shm_connection(io, SM_BUSY_TIMEOUT_MS);
    if (shm_is_raise(conn)) { ret = conn; conn = NULL; goto done; }
    json_t *params = json_array();
    json_append(params, json_string(SM_INSTALL_SNAPSHOT_KEY));
    json_t *spec = shm_db_execute(io, SQL_SELECT_VALUE, params);
    if (shm_is_raise(spec)) { ret = spec; goto done; }
    json_t *row = shm_cursor_one(spec);
    json_free(spec);
    if (!shm_is_null(row)) {
        json_t *val = shm_row_get(row, "value");
        if (shm_is_raise(val)) { json_free(row); ret = val; goto done; }
        json_t *pa = json_object();
        shm_arg_j(pa, "value", val);
        json_free(val);
        last = shm_call(io, "store._parse_state_timestamp", pa);
        if (shm_is_raise(last)) { json_free(row); ret = last; goto done; }
    }
    json_free(row);
    now = shm_call(io, "_utc_now", json_object());
    if (shm_is_raise(now)) { ret = now; now = NULL; goto done; }
    if (shm_is_null(last)) { ret = json_bool(true); goto done; }
    long long last_us, now_us;
    if (!shm_dt_parse(shm_str(last), &last_us)) {
        char *m = shm_xfmt("Invalid isoformat string: '%s'", shm_str(last) ? shm_str(last) : "");
        ret = shm_mkraise("ValueError", m ? m : "Invalid isoformat string"); free(m); goto done;
    }
    if (!shm_dt_parse(shm_str(now), &now_us)) {
        char *m = shm_xfmt("Invalid isoformat string: '%s'", shm_str(now) ? shm_str(now) : "");
        ret = shm_mkraise("ValueError", m ? m : "Invalid isoformat string"); free(m); goto done;
    }
    if (last_us > now_us || now_us >= last_us + SM_INTERVAL_US) ret = json_bool(true);
    else ret = json_bool(false);
done:
    json_free(conn); json_free(now); json_free(last);
    return ret;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 7. _record_once_per_interval — transactional 24h compare-and-set latch
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *shm_record_once_per_interval_core(shm_io_t *io, json_t *st,
    const char *state_key, const char *metric_name, const json_t *dimensions, const json_t *resource)
{
    json_t *ret = NULL, *conn = NULL, *now = NULL, *last = NULL;
    json_t *va = json_object();
    shm_arg_s(va, "metric_name", metric_name);
    shm_arg_j(va, "dimensions", dimensions);
    shm_arg_j(va, "resource", resource);
    json_t *v = shm_call(io, "store._validate_counter", va);
    if (shm_is_raise(v)) return v;
    json_free(v);
    now = shm_call(io, "_utc_now", json_object());
    if (shm_is_raise(now)) { ret = now; now = NULL; return ret; }
    json_t *w = shm_write(io, st, SM_BUSY_TIMEOUT_MS, &conn);
    if (w) { ret = w; goto done; }
    json_t *params = json_array();
    json_append(params, json_string(state_key));
    json_t *spec = shm_db_execute(io, SQL_SELECT_VALUE, params);
    if (shm_is_raise(spec)) { ret = spec; goto done; }
    json_t *row = shm_cursor_one(spec);
    json_free(spec);
    if (!shm_is_null(row)) {
        json_t *val = shm_row_get(row, "value");
        if (shm_is_raise(val)) { json_free(row); ret = val; goto done; }
        json_t *pa = json_object();
        shm_arg_j(pa, "value", val);
        json_free(val);
        last = shm_call(io, "store._parse_state_timestamp", pa);
        if (shm_is_raise(last)) { json_free(row); ret = last; goto done; }
    }
    json_free(row);
    if (!shm_is_null(last)) {
        long long last_us, now_us;
        if (!shm_dt_parse(shm_str(last), &last_us)) {
            char *m = shm_xfmt("Invalid isoformat string: '%s'", shm_str(last) ? shm_str(last) : "");
            ret = shm_mkraise("ValueError", m ? m : "Invalid isoformat string"); free(m); goto done;
        }
        if (!shm_dt_parse(shm_str(now), &now_us)) {
            char *m = shm_xfmt("Invalid isoformat string: '%s'", shm_str(now) ? shm_str(now) : "");
            ret = shm_mkraise("ValueError", m ? m : "Invalid isoformat string"); free(m); goto done;
        }
        if (last_us > now_us) {
            /* wall-clock correction: reset the stamp, suppress */
            json_t *ia = json_object(); shm_arg_j(ia, "value", now);
            json_t *iso = shm_call(io, "_isoformat", ia);
            if (shm_is_raise(iso)) { ret = iso; goto done; }
            json_t *up = json_array();
            json_append(up, iso);
            json_append(up, json_string(state_key));
            json_t *us = shm_db_execute(io, SQL_UPDATE_STATE, up);
            if (shm_is_raise(us)) { ret = us; goto done; }
            json_free(us);
            ret = json_bool(false); goto done;
        }
        if (now_us < last_us + SM_INTERVAL_US) { ret = json_bool(false); goto done; }
    }
    json_t *ii = json_object(); shm_arg_j(ii, "connection", conn);
    json_t *ir = shm_call(io, "store._install_id", ii);
    if (shm_is_raise(ir)) { ret = ir; goto done; }
    json_free(ir);
    char period[16]; shm_date_part(now, period);
    json_t *ra = json_object();
    shm_arg_j(ra, "connection", conn);
    shm_arg_s(ra, "metric_name", metric_name);
    shm_arg_j(ra, "dimensions", dimensions);
    shm_arg_j(ra, "resource", resource);
    shm_arg_s(ra, "period_start", period);
    json_t *rr = shm_call(io, "store._record_counter_in_transaction", ra);
    if (shm_is_raise(rr)) { ret = rr; goto done; }
    json_free(rr);
    json_t *ia2 = json_object(); shm_arg_j(ia2, "value", now);
    json_t *iso2 = shm_call(io, "_isoformat", ia2);
    if (shm_is_raise(iso2)) { ret = iso2; goto done; }
    json_t *ip = json_array();
    json_append(ip, json_string(state_key));
    json_append(ip, iso2);
    json_t *is = shm_db_execute(io, SQL_STATE_UPSERT, ip);
    if (shm_is_raise(is)) { ret = is; goto done; }
    json_free(is);
    ret = json_bool(true);
done:
    json_free(conn); json_free(now); json_free(last);
    return ret;
}
/* PoP: shm_record_once_per_interval @ hermes_cli/observability/shared_metrics.py:_record_once_per_interval */
static json_t *shm_record_once_per_interval(shm_io_t *io, const json_t *in, json_t *st)
{
    return shm_record_once_per_interval_core(io, st,
        json_get_str(in, "state_key", ""), json_get_str(in, "metric_name", ""),
        shm_get(in, "dimensions"), shm_get(in, "resource"));
}

/* ══════════════════════════════════════════════════════════════════════════
 * 8. record_install_snapshot — 24h latch wrapper (REAL composition)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_record_install_snapshot @ hermes_cli/observability/shared_metrics.py:record_install_snapshot */
static json_t *shm_record_install_snapshot(shm_io_t *io, const json_t *in, json_t *st)
{
    return shm_record_once_per_interval_core(io, st, SM_INSTALL_SNAPSHOT_KEY,
        SM_INSTALL_SNAPSHOT_METRIC, shm_get(in, "dimensions"), shm_get(in, "resource"));
}

/* ══════════════════════════════════════════════════════════════════════════
 * 9. _record_once_ever — permanent compare-and-set latch
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *shm_record_once_ever_core(shm_io_t *io, json_t *st,
    const char *state_key, const char *metric_name, const json_t *dimensions, const json_t *resource)
{
    json_t *ret = NULL, *conn = NULL, *now = NULL;
    json_t *va = json_object();
    shm_arg_s(va, "metric_name", metric_name);
    shm_arg_j(va, "dimensions", dimensions);
    shm_arg_j(va, "resource", resource);
    json_t *v = shm_call(io, "store._validate_counter", va);
    if (shm_is_raise(v)) return v;
    json_free(v);
    now = shm_call(io, "_utc_now", json_object());
    if (shm_is_raise(now)) { ret = now; now = NULL; return ret; }
    json_t *w = shm_write(io, st, SM_BUSY_TIMEOUT_MS, &conn);
    if (w) { ret = w; goto done; }
    json_t *ia = json_object(); shm_arg_j(ia, "value", now);
    json_t *iso = shm_call(io, "_isoformat", ia);
    if (shm_is_raise(iso)) { ret = iso; goto done; }
    json_t *ip = json_array();
    json_append(ip, json_string(state_key));
    json_append(ip, iso);
    json_t *spec = shm_db_execute(io, SQL_INSERT_NOTHING, ip);
    if (shm_is_raise(spec)) { ret = spec; goto done; }
    double inserted = shm_cursor_rowcount(spec);
    json_free(spec);
    if (inserted == 0.0) { ret = json_bool(false); goto done; }
    json_t *ii = json_object(); shm_arg_j(ii, "connection", conn);
    json_t *ir = shm_call(io, "store._install_id", ii);
    if (shm_is_raise(ir)) { ret = ir; goto done; }
    json_free(ir);
    char period[16]; shm_date_part(now, period);
    json_t *ra = json_object();
    shm_arg_j(ra, "connection", conn);
    shm_arg_s(ra, "metric_name", metric_name);
    shm_arg_j(ra, "dimensions", dimensions);
    shm_arg_j(ra, "resource", resource);
    shm_arg_s(ra, "period_start", period);
    json_t *rr = shm_call(io, "store._record_counter_in_transaction", ra);
    if (shm_is_raise(rr)) { ret = rr; goto done; }
    json_free(rr);
    ret = json_bool(true);
done:
    json_free(conn); json_free(now);
    return ret;
}
/* PoP: shm_record_once_ever @ hermes_cli/observability/shared_metrics.py:_record_once_ever */
static json_t *shm_record_once_ever(shm_io_t *io, const json_t *in, json_t *st)
{
    return shm_record_once_ever_core(io, st,
        json_get_str(in, "state_key", ""), json_get_str(in, "metric_name", ""),
        shm_get(in, "dimensions"), shm_get(in, "resource"));
}

/* ══════════════════════════════════════════════════════════════════════════
 * 10. record_milestone — permanent latch wrapper (REAL composition)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_record_milestone @ hermes_cli/observability/shared_metrics.py:record_milestone */
static json_t *shm_record_milestone(shm_io_t *io, const json_t *in, json_t *st)
{
    const char *milestone = json_get_str(in, "milestone", "");
    const char *bucket = json_get_str(in, "install_age_bucket", "");
    json_t *dims = json_object();
    json_set(dims, "install_age_bucket", json_string(bucket));
    json_set(dims, "milestone", json_string(milestone));
    char *key = shm_xfmt("%s%s", SM_MILESTONE_PREFIX, milestone);
    json_t *r = shm_record_once_ever_core(io, st, key ? key : SM_MILESTONE_PREFIX,
                                        SM_MILESTONE_METRIC, dims, shm_get(in, "resource"));
    free(key);
    json_free(dims);
    return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 11. record_feature_adoption — permanent latch wrapper (REAL composition)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_record_feature_adoption @ hermes_cli/observability/shared_metrics.py:record_feature_adoption */
static json_t *shm_record_feature_adoption(shm_io_t *io, const json_t *in, json_t *st)
{
    const char *feature = json_get_str(in, "feature", "");
    const char *bucket = json_get_str(in, "days_bucket", "");
    json_t *dims = json_object();
    json_set(dims, "days_since_install_bucket", json_string(bucket));
    json_set(dims, "feature", json_string(feature));
    char *key = shm_xfmt("%s%s", SM_FEATURE_PREFIX, feature);
    json_t *r = shm_record_once_ever_core(io, st, key ? key : SM_FEATURE_PREFIX,
                                        SM_FEATURE_METRIC, dims, shm_get(in, "resource"));
    free(key);
    json_free(dims);
    return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 12. _latched — frozenset of state-key suffixes under a prefix
 * ══════════════════════════════════════════════════════════════════════════ */
static json_t *shm_latched_core(shm_io_t *io, json_t *st, const char *prefix)
{
    (void)st;
    json_t *ret = NULL, *conn = NULL;
    conn = shm_connection(io, SM_BUSY_TIMEOUT_MS);
    if (shm_is_raise(conn)) { ret = conn; conn = NULL; goto done; }
    char *like = shm_xfmt("%s%%", prefix);
    json_t *params = json_array();
    json_append(params, json_string(like ? like : "%"));
    free(like);
    json_t *spec = shm_db_execute(io, SQL_SELECT_KEY_LIKE, params);
    if (shm_is_raise(spec)) { ret = spec; goto done; }
    json_t *all = shm_cursor_all(spec);
    json_free(spec);
    json_t *suffixes = json_array();
    size_t plen = strlen(prefix);
    for (size_t i = 0; i < all->c.count; i++) {
        json_t *k = shm_row_get(all->c.items[i], "key");
        if (shm_is_raise(k)) { json_free(all); ret = k; goto done; }
        if (!shm_is_str(k)) {
            char *m = shm_xfmt("'%s' object is not subscriptable", shm_type_name(k));
            json_free(k); json_free(all);
            ret = shm_mkraise("TypeError", m ? m : "not subscriptable"); free(m); goto done;
        }
        const char *ks = k->str_val ? k->str_val : "";
        const char *suf = (strlen(ks) >= plen) ? ks + plen : "";
        bool found = false;
        for (size_t j = 0; j < suffixes->c.count; j++) {
            if (shm_str_eq(shm_str(suffixes->c.items[j]), suf)) { found = true; break; }
        }
        if (!found) json_append(suffixes, json_string(suf));
        json_free(k);
    }
    json_free(all);
    /* sort (frozenset normalizes to a sorted list on both sides) */
    for (size_t i = 1; i < suffixes->c.count; i++) {
        json_t *key = suffixes->c.items[i];
        size_t j = i;
        while (j > 0 && strcmp(shm_str(suffixes->c.items[j - 1]) ? shm_str(suffixes->c.items[j - 1]) : "",
                               shm_str(key) ? shm_str(key) : "") > 0) {
            suffixes->c.items[j] = suffixes->c.items[j - 1]; j--;
        }
        suffixes->c.items[j] = key;
    }
    ret = suffixes;
done:
    json_free(conn);
    return ret;
}
/* PoP: shm_latched @ hermes_cli/observability/shared_metrics.py:_latched */
static json_t *shm_latched(shm_io_t *io, const json_t *in, json_t *st)
{
    return shm_latched_core(io, st, json_get_str(in, "prefix", ""));
}

/* ══════════════════════════════════════════════════════════════════════════
 * 13. recorded_milestones (REAL composition -> _latched)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_recorded_milestones @ hermes_cli/observability/shared_metrics.py:recorded_milestones */
static json_t *shm_recorded_milestones(shm_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    return shm_latched_core(io, st, SM_MILESTONE_PREFIX);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 14. recorded_features (REAL composition -> _latched)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_recorded_features @ hermes_cli/observability/shared_metrics.py:recorded_features */
static json_t *shm_recorded_features(shm_io_t *io, const json_t *in, json_t *st)
{
    (void)in;
    return shm_latched_core(io, st, SM_FEATURE_PREFIX);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 15. record_counter_once_per_day — daily per-dimension-set latch
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_record_counter_once_per_day @ hermes_cli/observability/shared_metrics.py:record_counter_once_per_day */
static json_t *shm_record_counter_once_per_day(shm_io_t *io, const json_t *in, json_t *st)
{
    const char *metric_name = json_get_str(in, "metric_name", "");
    const json_t *dimensions = shm_get(in, "dimensions");
    const json_t *resource = shm_get(in, "resource");
    json_t *ret = NULL, *conn = NULL, *now = NULL;
    char *key = NULL;
    json_t *va = json_object();
    shm_arg_s(va, "metric_name", metric_name);
    shm_arg_j(va, "dimensions", dimensions);
    shm_arg_j(va, "resource", resource);
    json_t *v = shm_call(io, "store._validate_counter", va);
    if (shm_is_raise(v)) return v;
    json_free(v);
    now = shm_call(io, "_utc_now", json_object());
    if (shm_is_raise(now)) { ret = now; now = NULL; return ret; }
    if (!shm_is_str(now)) {
        ret = shm_mkraise("AttributeError", "'NoneType' object has no attribute 'date'");
        goto done;
    }
    char today[16]; shm_date_part(now, today);
    json_t *dn = json_object();
    if (shm_is_obj(dimensions)) {
        for (size_t i = 0; i < dimensions->c.count; i++) {
            if (strcmp(dimensions->c.keys[i], "surface") != 0)
                json_set(dn, dimensions->c.keys[i], json_copy(dimensions->c.items[i]));
        }
    }
    char *cj = shm_compact_json(dn);
    json_free(dn);
    key = shm_xfmt("%s%s:%s", SM_DAILY_ONCE_PREFIX, metric_name, cj ? cj : "");
    free(cj);
    json_t *w = shm_write(io, st, SM_BUSY_TIMEOUT_MS, &conn);
    if (w) { ret = w; goto done; }
    json_t *params = json_array();
    json_append(params, json_string(key ? key : ""));
    json_append(params, json_string(today));
    json_t *spec = shm_db_execute(io, SQL_DAILY_UPSERT, params);
    if (shm_is_raise(spec)) { ret = spec; goto done; }
    double changed = shm_cursor_rowcount(spec);
    json_free(spec);
    if (changed == 0.0) { ret = json_bool(false); goto done; }
    json_t *ii = json_object(); shm_arg_j(ii, "connection", conn);
    json_t *ir = shm_call(io, "store._install_id", ii);
    if (shm_is_raise(ir)) { ret = ir; goto done; }
    json_free(ir);
    json_t *ra = json_object();
    shm_arg_j(ra, "connection", conn);
    shm_arg_s(ra, "metric_name", metric_name);
    shm_arg_j(ra, "dimensions", dimensions);
    shm_arg_j(ra, "resource", resource);
    shm_arg_s(ra, "period_start", today);
    json_t *rr = shm_call(io, "store._record_counter_in_transaction", ra);
    if (shm_is_raise(rr)) { ret = rr; goto done; }
    json_free(rr);
    ret = json_bool(true);
done:
    json_free(conn); json_free(now); free(key);
    return ret;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 16. _update_rollup_state — read-modify-write one JSON rollup state
 * ══════════════════════════════════════════════════════════════════════════ */
/* unpack a 2-sequence result into (new_state, rows); exact py3.14 messages.
 * result owned (consumed). On success *ns/*rows owned copies. */
static json_t *shm_unpack2(json_t *result, json_t **ns, json_t **rows)
{
    *ns = NULL; *rows = NULL;
    if (shm_is_arr(result)) {
        size_t n = result->c.count;
        if (n == 2) {
            *ns = json_copy(result->c.items[0]);
            *rows = json_copy(result->c.items[1]);
            json_free(result);
            return NULL;
        }
        char *m = shm_xfmt("%s values to unpack (expected 2, got %zu)",
                           n > 2 ? "too many" : "not enough", n);
        json_t *e = shm_mkraise("ValueError", m ? m : "unpack"); free(m);
        json_free(result);
        return e;
    }
    char *m = shm_xfmt("cannot unpack non-iterable %s object", shm_type_name(result));
    json_t *e = shm_mkraise("TypeError", m ? m : "unpack"); free(m);
    json_free(result);
    return e;
}
static json_t *shm_update_rollup_state_core(shm_io_t *io, json_t *st, const json_t *conn,
                                            const char *state_key, const json_t *update)
{
    (void)st;
    json_t *ret = NULL, *ns = NULL, *rows = NULL, *valid = NULL;
    json_t *params = json_array();
    json_append(params, json_string(state_key));
    json_t *spec = shm_db_execute(io, SQL_SELECT_VALUE, params);
    if (shm_is_raise(spec)) return spec;
    json_t *row = shm_cursor_one(spec);
    json_free(spec);
    json_t *state = NULL;
    if (!shm_is_null(row)) {
        json_t *val = shm_row_get(row, "value");
        if (shm_is_raise(val)) { json_free(row); return val; }
        if (shm_is_str(val)) {
            char *err = NULL;
            state = json_parse(val->str_val ? val->str_val : "", &err);
            free(err);   /* TypeError/ValueError swallowed -> state stays NULL */
        }
        json_free(val);
    }
    json_free(row);
    /* update(state if isinstance(state, dict) else None) */
    json_t *args = json_array();
    json_append(args, shm_is_obj(state) ? json_copy(state) : json_null());
    json_free(state);
    json_t *result = shm_invoke_callable(io, update, args, json_object());
    if (shm_is_raise(result)) return result;
    json_t *ue = shm_unpack2(result, &ns, &rows);
    if (ue) return ue;
    /* valid = [r for r in rows if dims_valid(r[0], r[1]) and resource_valid(r[2])] */
    valid = json_array();
    if (!shm_is_arr(rows)) {
        char *m = shm_xfmt("'%s' object is not iterable", shm_type_name(rows));
        ret = shm_mkraise("TypeError", m ? m : "not iterable"); free(m); goto done;
    }
    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *r = rows->c.items[i];
        json_t *m = shm_index_get(r, 0);
        if (shm_is_raise(m)) { ret = m; goto done; }
        json_t *d = shm_index_get(r, 1);
        if (shm_is_raise(d)) { json_free(m); ret = d; goto done; }
        json_t *res = shm_index_get(r, 2);
        if (shm_is_raise(res)) { json_free(m); json_free(d); ret = res; goto done; }
        json_t *da = json_object();
        shm_arg_j(da, "metric_name", m);
        shm_arg_j(da, "dimensions", d);
        json_t *dv = shm_call(io, "contract.counter_dimensions_are_valid", da);
        if (shm_is_raise(dv)) { json_free(m); json_free(d); json_free(res); ret = dv; goto done; }
        bool ok = shm_truth(dv);
        json_free(dv);
        if (ok) {
            json_t *rva = json_object();
            shm_arg_j(rva, "resource", res);
            json_t *rv = shm_call(io, "contract.client_resource_is_valid", rva);
            if (shm_is_raise(rv)) { json_free(m); json_free(d); json_free(res); ret = rv; goto done; }
            ok = shm_truth(rv);
            json_free(rv);
        }
        if (ok) json_append(valid, json_copy(r));
        json_free(m); json_free(d); json_free(res);
    }
    if (valid->c.count > 0) {
        json_t *ii = json_object(); shm_arg_j(ii, "connection", conn);
        json_t *ir = shm_call(io, "store._install_id", ii);
        if (shm_is_raise(ir)) { ret = ir; goto done; }
        json_free(ir);
    }
    for (size_t i = 0; i < valid->c.count; i++) {
        const json_t *r = valid->c.items[i];
        if (!shm_is_arr(r) || r->c.count != 4) {
            if (shm_is_arr(r)) {
                size_t n = r->c.count;
                char *m = shm_xfmt("%s values to unpack (expected 4, got %zu)",
                                   n > 4 ? "too many" : "not enough", n);
                ret = shm_mkraise("ValueError", m ? m : "unpack"); free(m);
            } else {
                char *m = shm_xfmt("cannot unpack non-iterable %s object", shm_type_name(r));
                ret = shm_mkraise("TypeError", m ? m : "unpack"); free(m);
            }
            goto done;
        }
        json_t *ra = json_object();
        shm_arg_j(ra, "connection", conn);
        shm_arg_j(ra, "metric_name", r->c.items[0]);
        shm_arg_j(ra, "dimensions", r->c.items[1]);
        shm_arg_j(ra, "resource", r->c.items[2]);
        shm_arg_j(ra, "period_start", r->c.items[3]);
        json_t *rr = shm_call(io, "store._record_counter_in_transaction", ra);
        if (shm_is_raise(rr)) { ret = rr; goto done; }
        json_free(rr);
    }
    char *cj = shm_compact_json(ns);
    json_t *up = json_array();
    json_append(up, json_string(state_key));
    json_append(up, json_string(cj ? cj : "null"));
    free(cj);
    json_t *us = shm_db_execute(io, SQL_STATE_UPSERT, up);
    if (shm_is_raise(us)) { ret = us; goto done; }
    json_free(us);
    ret = NULL;
done:
    json_free(ns); json_free(rows); json_free(valid);
    return ret;
}
/* PoP: shm_update_rollup_state @ hermes_cli/observability/shared_metrics.py:_update_rollup_state */
static json_t *shm_update_rollup_state(shm_io_t *io, const json_t *in, json_t *st)
{
    return shm_update_rollup_state_core(io, st, shm_get(in, "connection"),
        json_get_str(in, "state_key", ""), shm_get(in, "update"));
}

/* ══════════════════════════════════════════════════════════════════════════
 * 17. _write_or_defer — run pending writes; busy OperationalError defers
 * ══════════════════════════════════════════════════════════════════════════ */
/* append the last-N truncation of (a ++ b) into st._deferred. a/b borrowed. */
static void shm_deferred_assign(json_t *st, const json_t *a, const json_t *b)
{
    json_t *merged = json_array();
    if (shm_is_arr(a)) for (size_t i = 0; i < a->c.count; i++) json_append(merged, json_copy(a->c.items[i]));
    if (shm_is_arr(b)) for (size_t i = 0; i < b->c.count; i++) json_append(merged, json_copy(b->c.items[i]));
    size_t n = merged->c.count;
    if (n > SM_DEFERRED_WRITES_MAX) {
        size_t drop = n - SM_DEFERRED_WRITES_MAX;
        for (size_t i = 0; i < drop; i++) json_free(merged->c.items[i]);
        for (size_t i = drop; i < n; i++) merged->c.items[i - drop] = merged->c.items[i];
        merged->c.count = SM_DEFERRED_WRITES_MAX;
    }
    json_set(st, "_deferred", merged);
}
static json_t *shm_write_or_defer_core(shm_io_t *io, json_t *st, const json_t *write_cb)
{
    /* pending, self._deferred = [*self._deferred, write], [] */
    json_t *pending = json_array();
    const json_t *cur = shm_get(st, "_deferred");
    if (shm_is_arr(cur)) for (size_t i = 0; i < cur->c.count; i++) json_append(pending, json_copy(cur->c.items[i]));
    json_append(pending, json_copy(write_cb));
    json_set(st, "_deferred", json_array());

    json_t *ret = NULL, *conn = NULL, *err = NULL;
    json_t *w = shm_write(io, st, SM_BUSY_TIMEOUT_MS, &conn);
    if (w) { err = w; goto except_op; }
    for (size_t i = 0; i < pending->c.count; i++) {
        json_t *r = shm_invoke_deferred(io, st, pending->c.items[i], conn);
        if (shm_is_raise(r)) { err = r; goto except_op; }
        json_free(r);
    }
    json_free(conn);
    json_free(pending);
    return NULL;

except_op:
    json_free(conn);
    /* except sqlite3.OperationalError: only that kind is caught */
    if (!shm_str_eq(shm_kind(err), "OperationalError")) {
        json_free(pending);
        return err;   /* other Exception / BaseException propagates */
    }
    const char *msg = shm_str(shm_get(err, "message"));
    bool busy = (msg && strstr(msg, "locked")) || (msg && strstr(msg, "busy"));
    /* self._deferred = [*(pending if busy else pending[:-1]), *self._deferred][-MAX:] */
    json_t *keep = json_array();
    size_t pn = pending->c.count;
    size_t keep_n = busy ? pn : (pn > 0 ? pn - 1 : 0);
    for (size_t i = 0; i < keep_n; i++) json_append(keep, json_copy(pending->c.items[i]));
    json_free(pending);
    const json_t *cur2 = shm_get(st, "_deferred");
    shm_deferred_assign(st, keep, cur2);
    json_free(keep);
    /* register, self._drain_at_exit = not self._drain_at_exit, True */
    bool drain = shm_bool(st, "_drain_at_exit", false);
    bool regist = !drain;
    json_set(st, "_drain_at_exit", json_bool(true));
    if (regist) {
        json_t *cb = json_object();
        json_set(cb, "__callable__", json_string("store.drain_deferred"));
        json_t *aa = json_object();
        shm_arg_j(aa, "callback", cb);
        json_free(cb);
        json_t *ar = shm_call(io, "atexit.register", aa);
        if (shm_is_raise(ar)) { json_free(err); return ar; }
        json_free(ar);
        json_t *ds = shm_get(st, "_DEFERRING_STORES");
        if (!shm_is_arr(ds)) { ds = json_array(); json_set(st, "_DEFERRING_STORES", ds); }
        json_t *self_marker = json_object();
        json_set(self_marker, "id", json_string(json_get_str(st, "id", "")));
        json_append(ds, self_marker);
    }
    if (!busy) return err;   /* re-raise the non-contention OperationalError */
    json_free(err);
    return NULL;
}
/* PoP: shm_write_or_defer @ hermes_cli/observability/shared_metrics.py:_write_or_defer */
static json_t *shm_write_or_defer(shm_io_t *io, const json_t *in, json_t *st)
{
    return shm_write_or_defer_core(io, st, shm_get(in, "write"));
}

/* ══════════════════════════════════════════════════════════════════════════
 * 18. update_rollup_state — defer the rollup lambda (REAL composition)
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_update_rollup_state_entry @ hermes_cli/observability/shared_metrics.py:update_rollup_state */
static json_t *shm_update_rollup_state_entry(shm_io_t *io, const json_t *in, json_t *st)
{
    json_t *cb = json_object();
    json_set(cb, "__fn__", json_string("_update_rollup_state"));
    json_set(cb, "state_key", json_string(json_get_str(in, "state_key", "")));
    const json_t *upd = shm_get(in, "update");
    json_set(cb, "update", upd ? json_copy(upd) : json_null());
    json_t *r = shm_write_or_defer_core(io, st, cb);
    json_free(cb);
    return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 19. drain_deferred — write deferred rows (exit / export); busy keeps them
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_drain_deferred @ hermes_cli/observability/shared_metrics.py:drain_deferred */
static json_t *shm_drain_deferred(shm_io_t *io, const json_t *in, json_t *st)
{
    double bt = shm_num(in, "busy_timeout_ms", SM_SCHEMA_BUSY_TIMEOUT_MS);
    /* pending, self._deferred = self._deferred, [] */
    json_t *pending = json_array();
    const json_t *cur = shm_get(st, "_deferred");
    if (shm_is_arr(cur)) for (size_t i = 0; i < cur->c.count; i++) json_append(pending, json_copy(cur->c.items[i]));
    json_set(st, "_deferred", json_array());
    size_t pending_n = pending->c.count;
    if (pending_n == 0) { json_free(pending); return NULL; }

    json_t *conn = NULL, *err = NULL;
    json_t *w = shm_write(io, st, bt, &conn);
    if (w) { err = w; goto except_exc; }
    for (size_t i = 0; i < pending->c.count; i++) {
        json_t *r = shm_invoke_deferred(io, st, pending->c.items[i], conn);
        if (shm_is_raise(r)) { err = r; goto except_exc; }
        json_free(r);
    }
    json_free(conn);
    json_free(pending);
    return NULL;

except_exc:
    json_free(conn);
    /* except Exception: BaseException trio passes through */
    if (shm_is_base(shm_kind(err))) { json_free(pending); return err; }
    const char *msg = shm_str(shm_get(err, "message"));
    bool busy = shm_str_eq(shm_kind(err), "OperationalError")
             && ((msg && strstr(msg, "locked")) || (msg && strstr(msg, "busy")));
    if (busy) {
        const json_t *cur2 = shm_get(st, "_deferred");
        shm_deferred_assign(st, pending, cur2);
        json_free(pending);
        json_free(err);
        return NULL;
    }
    json_free(pending);
    json_free(err);
    /* logger.warning("Unable to write %d deferred shared-metrics updates", len(pending), exc_info=True) */
    json_t *la = json_array();
    json_append(la, json_number((double)pending_n));
    json_t *lr = shm_log(io, "warning", "Unable to write %d deferred shared-metrics updates", la, true);
    if (shm_is_raise(lr)) return lr;
    json_free(lr);
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 20. _mark_packaged (staticmethod) — settle the data high-water mark per row
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_mark_packaged @ hermes_cli/observability/shared_metrics.py:_mark_packaged */
static json_t *shm_mark_packaged(shm_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *conn = shm_get(in, "connection");
    const json_t *rows = shm_get(in, "rows");
    const char *period_value = json_get_str(in, "period_value", "");
    const json_t *rv = shm_get(in, "resource_values");
    if (!shm_is_arr(rows)) return NULL;
    for (size_t i = 0; i < rows->c.count; i++) {
        const json_t *row = rows->c.items[i];
        json_t *mn = shm_row_get(row, "metric_name");
        if (shm_is_raise(mn)) return mn;
        json_t *dj = shm_row_get(row, "dimensions_json");
        if (shm_is_raise(dj)) { json_free(mn); return dj; }
        json_t *params = json_array();
        json_append(params, json_string(period_value));
        json_append(params, mn);
        if (shm_is_arr(rv)) for (size_t j = 0; j < rv->c.count; j++) json_append(params, json_copy(rv->c.items[j]));
        json_append(params, dj);
        json_t *spec = shm_db_execute(io, SQL_UPDATE_PACKAGED, params);
        if (shm_is_raise(spec)) return spec;
        json_free(spec);
    }
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════
 * 21. _package_metrics (classmethod) — merge re-bucketed rows per dimensions
 * ══════════════════════════════════════════════════════════════════════════ */
/* PoP: shm_package_metrics @ hermes_cli/observability/shared_metrics.py:_package_metrics */
static json_t *shm_package_metrics(shm_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *rows = shm_get(in, "rows");
    json_t *merged = json_array();   /* metric dicts in first-occurrence order */
    json_t *keys = json_array();     /* parallel key strings */
    if (!shm_is_arr(rows)) return merged;
    for (size_t i = 0; i < rows->c.count; i++) {
        json_t *pa = json_object();
        shm_arg_j(pa, "row", rows->c.items[i]);
        json_t *metric = shm_call(io, "store._package_metric", pa);
        if (shm_is_raise(metric)) { json_free(merged); json_free(keys); return metric; }
        if (shm_is_null(metric)) { json_free(metric); continue; }
        /* key = (metric["name"], _compact_json(metric["dimensions"])) */
        json_t *name = shm_get(metric, "name");
        if (!name) { json_free(metric); json_free(merged); json_free(keys); return shm_mkraise("KeyError", "'name'"); }
        json_t *dims = shm_get(metric, "dimensions");
        if (!dims) { json_free(metric); json_free(merged); json_free(keys); return shm_mkraise("KeyError", "'dimensions'"); }
        char *cj = shm_compact_json(dims);
        char *key = shm_xfmt("%zu:%s%s", strlen(shm_str(name) ? shm_str(name) : ""),
                             shm_str(name) ? shm_str(name) : "", cj ? cj : "");
        free(cj);
        size_t at = (size_t)-1;
        for (size_t j = 0; j < keys->c.count; j++) {
            if (shm_str_eq(shm_str(keys->c.items[j]), key)) { at = j; break; }
        }
        if (at != (size_t)-1) {
            json_t *existing = merged->c.items[at];
            json_t *ev = shm_get(existing, "value");
            json_t *mv = shm_get(metric, "value");
            if (!ev) { free(key); json_free(metric); json_free(merged); json_free(keys); return shm_mkraise("KeyError", "'value'"); }
            if (!mv) { free(key); json_free(metric); json_free(merged); json_free(keys); return shm_mkraise("KeyError", "'value'"); }
            if (!shm_is_num(ev) || !shm_is_num(mv)) {
                char *m = shm_xfmt("unsupported operand type(s) for +=: '%s' and '%s'",
                                   shm_type_name(ev), shm_type_name(mv));
                json_t *e = shm_mkraise("TypeError", m ? m : "unsupported operand"); free(m);
                free(key); json_free(metric); json_free(merged); json_free(keys);
                return e;
            }
            json_set(existing, "value", json_number(ev->num_val + mv->num_val));
            json_free(metric);
        } else {
            json_append(keys, json_string(key));
            json_append(merged, metric);   /* transfer ownership */
        }
        free(key);
    }
    json_free(keys);
    return merged;
}
