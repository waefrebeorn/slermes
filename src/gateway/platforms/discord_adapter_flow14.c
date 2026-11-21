/*
 * discord_adapter_flow14.c - Discord adapter FLOW part 14 of
 * plugins/platforms/discord/adapter.py (hermes-agent): the record/register/
 * refresh/reject remainder batch left after discord_adapter_flow1-13.
 * Every target below was confirmed UNCLAIMED.
 *
 * Themes: command-sync ledgers, missed-message recovery ledger writers,
 * response recording, recovery scans, skill catalog / slash registration and
 * unauthorized-slash rejection. All live I/O and injected callables enter
 * through ONE universal seam: da14_call(ctx, name, args).
 *
 * Self-contained: harness #includes this .c directly; libjson only.
 * Every symbol is static and da14_-prefixed.
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow14.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - Except sites swallow ONLY Exception-family kinds (DA14_EXC_HIER);
 *   BaseException-only kinds pass through every except-Exception site.
 * - Integral doubles render as ints in py_str (and canon folds them too).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DA14_TEXT_LIMIT 100
#define DA14_MAX_APP_COMMANDS 100

/* SQL, whitespace-normalized identically to the oracle's " ".join(sql.split()) */
#define SQL_SEL_STATUS "SELECT status, updated_at FROM discord_messages WHERE message_id=?"
#define SQL_INS_SEEN "INSERT INTO discord_messages (message_id, channel_id, thread_id, parent_channel_id, author_id, created_at, status, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(message_id) DO UPDATE SET channel_id=excluded.channel_id, thread_id=excluded.thread_id, parent_channel_id=excluded.parent_channel_id, author_id=excluded.author_id, created_at=COALESCE(discord_messages.created_at, excluded.created_at), status=?, updated_at=excluded.updated_at"
#define SQL_UPD_ATTEMPT "UPDATE discord_messages SET status=?, attempts=attempts+?, last_attempt_at=?, last_error=?, updated_at=? WHERE message_id=?"
#define SQL_UPD_START "UPDATE discord_messages SET status='processing', emoji_ack=?, updated_at=? WHERE message_id=?"
#define SQL_UPD_SUCCESS "UPDATE discord_messages SET status='responded', replied=1, updated_at=? WHERE message_id=?"
#define SQL_UPD_CASE "UPDATE discord_messages SET status=CASE WHEN status='responded' THEN status ELSE ? END, updated_at=? WHERE message_id=?"
#define SQL_INS_RESP "INSERT INTO discord_messages (message_id, status, replied, outage_response, response_message_id, updated_at) VALUES (?, ?, ?, 0, ?, ?) ON CONFLICT(message_id) DO UPDATE SET status=CASE WHEN ? THEN 'responded' ELSE discord_messages.status END, replied=CASE WHEN ? THEN 1 ELSE discord_messages.replied END, outage_response=CASE WHEN ? THEN 0 ELSE discord_messages.outage_response END, response_message_id=COALESCE(?, response_message_id), updated_at=?"
#define SQL_SEL_CHANNEL "SELECT COALESCE(thread_id, channel_id) FROM discord_messages WHERE message_id=?"
#define SQL_INS_SCAN "INSERT OR REPLACE INTO discord_recovery_scans (scan_id, started_at, status, channels, window_seconds, limit_count) VALUES (?, ?, ?, ?, ?, ?)"
#define SQL_UPD_SCAN "UPDATE discord_recovery_scans SET completed_at=?, status=?, scanned=?, missed=?, dispatched=?, error=? WHERE scan_id=?"

typedef struct da14_ctx {
    json_t *state;
    const json_t *io;
    json_t *cursors;
    json_t *trace;
} da14_ctx_t;

/* ── basic helpers ── */
static bool da14_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *da14_raise_kind(const json_t *v) {
    return da14_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool da14_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool da14_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *da14_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *da14_copyv(const json_t *v) { return v ? json_copy((json_t *)v) : json_null(); }
static json_t *da14_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const json_t *da14_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const char *da14_arg_str(const json_t *args, const char *key, const char *dflt) {
    const json_t *v = da14_arg(args, key);
    return da14_str(v) ? da14_str(v) : dflt;
}
static const json_t *da14_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *da14_state_get(const da14_ctx_t *ctx, const char *key) {
    return (ctx && ctx->state && ctx->state->type == JSON_OBJECT)
        ? json_obj_get(ctx->state, key) : NULL;
}
static void da14_state_set(da14_ctx_t *ctx, const char *key, json_t *v) {
    if (ctx && ctx->state && ctx->state->type == JSON_OBJECT) json_set(ctx->state, key, v);
}
static double da14_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static bool da14_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static bool da14_json_eq(const json_t *a, const json_t *b) {
    if (a == b) return true;
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL: return true;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return a->str_val && b->str_val && strcmp(a->str_val, b->str_val) == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!da14_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                if (!a->c.keys[i] || !b->c.keys[i] || strcmp(a->c.keys[i], b->c.keys[i]) != 0)
                    return false;
                if (!da14_json_eq(a->c.items[i], b->c.items[i])) return false;
            }
            return true;
        default: return false;
    }
}
static char *da14_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *da14_lower(const char *s) {
    char *o = da14_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *da14_trim(const char *s) {
    if (!s) return da14_strdup("");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    char *o = malloc((size_t)(b - a) + 1);
    if (!o) return da14_strdup("");
    memcpy(o, a, (size_t)(b - a)); o[b - a] = '\0';
    return o;
}
/* Python str() of a scalar; integral doubles render as ints. */
static char *da14_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da14_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return da14_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) {
                char t[64]; snprintf(t, sizeof t, "%.0f", d); return da14_strdup(t);
            }
            char t[64]; snprintf(t, sizeof t, "%.17g", d); return da14_strdup(t);
        }
        case JSON_STRING: return da14_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : da14_strdup(""); }
    }
}
/* str(getattr(obj, key, "") or "") - empty for null/empty. */
static char *da14_id_str(const json_t *obj, const char *key) {
    const json_t *v = da14_getattr(obj, key);
    if (da14_is_null(v)) return da14_strdup("");
    char *s = da14_py_str(v);
    if (s && s[0] == '\0') { free(s); return da14_strdup(""); }
    return s ? s : da14_strdup("");
}
static bool da14_yes(const char *s) {
    if (!s) return false;
    char *t = da14_trim(s);
    char *l = da14_lower(t);
    bool r = (strcmp(l, "true") == 0 || strcmp(l, "1") == 0 ||
              strcmp(l, "yes") == 0 || strcmp(l, "on") == 0);
    free(l); free(t);
    return r;
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } da14_exc_row_t;
static const da14_exc_row_t DA14_EXC_HIER[] = {
    {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"}, {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"}, {"BufferError", "Exception"},
    {"BytesWarning", "Warning"}, {"CancelledError", "BaseException"},
    {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"}, {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"}, {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"}, {"Exception", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"}, {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"}, {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"}, {"GeneratorExit", "BaseException"},
    {"ImportError", "Exception"}, {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"}, {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"}, {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"}, {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"}, {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"}, {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"}, {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"}, {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"}, {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"}, {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"}, {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"}, {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"}, {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"}, {"SystemError", "Exception"},
    {"SystemExit", "BaseException"}, {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"}, {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"}, {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"}, {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"}, {"ValueError", "Exception"}, {"Warning", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"}, {"_IncompleteInputError", "SyntaxError"},
};
static const char *da14_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(DA14_EXC_HIER) / sizeof(DA14_EXC_HIER[0]); i++)
        if (strcmp(name, DA14_EXC_HIER[i].name) == 0) return DA14_EXC_HIER[i].parent;
    return NULL;
}
static bool da14_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = da14_builtin_parent(cur);
    }
    return false;
}
static bool da14_is_exception(const char *kind) {
    if (!kind) return false;
    if (da14_builtin_parent(kind) == NULL) return true;
    return da14_chain_has(kind, "Exception");
}

/* ── the one universal seam ── */
static json_t *da14_call(da14_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256]; snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da14_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256]; snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da14_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            const json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = da14_err(json_get_str(raise, "type", "RuntimeError"),
                               json_get_str(raise, "message", ""));
            } else {
                const json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy((json_t *)r) : json_null();
            }
        } else {
            ret = used ? json_copy((json_t *)used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* helpers */
static json_t *da14_exec(da14_ctx_t *ctx, const char *sql, json_t *params) {
    json_t *a = json_object();
    json_set(a, "sql", json_string(sql));
    json_set(a, "params", params ? params : json_array());
    return da14_call(ctx, "conn_execute", a);
}
static json_t *da14_fetchone(da14_ctx_t *ctx) {
    return da14_call(ctx, "conn_fetchone", json_object());
}
static json_t *da14_with_db(da14_ctx_t *ctx) {
    return da14_call(ctx, "with_recovery_db", json_object());
}
static json_t *da14_logarr(json_t *arr) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    return a;
}
static json_t *da14_logkw(json_t *arr) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    json_set(a, "kwargs", json_object());
    return a;
}
static json_t *da14_arr_str(const char **s, size_t n) {
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(s[i] ? s[i] : ""));
    return a;
}
/* stable insertion-order copy of a JSON array of strings, sorted */
static json_t *da14_sorted_strings(const json_t *src) {
    json_t *a = json_array();
    if (src && src->type == JSON_ARRAY)
        for (size_t i = 0; i < src->c.count; i++)
            json_append(a, json_string(da14_str(src->c.items[i]) ? da14_str(src->c.items[i]) : ""));
    for (size_t i = 0; i < a->c.count; i++)
        for (size_t j = i + 1; j < a->c.count; j++)
            if (strcmp(a->c.items[i]->str_val, a->c.items[j]->str_val) > 0) {
                json_t *t = a->c.items[i]; a->c.items[i] = a->c.items[j]; a->c.items[j] = t;
            }
    return a;
}
static bool da14_arr_has(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (da14_str(arr->c.items[i]) && strcmp(da14_str(arr->c.items[i]), s) == 0) return true;
    return false;
}
static char *da14_fmt(const char *fmt, ...) {
    char tmp[4096]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return da14_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return da14_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}

/* ═══════════════════════ ported flows ═══════════════════════ */

/* PoP: da14_record_command_sync_attempt @ plugins/platforms/discord/adapter.py:_record_command_sync_attempt */
static json_t *da14_record_command_sync_attempt(da14_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "app_id", da14_copyv(da14_arg(args, "app_id")));
    const json_t *fp = da14_arg(args, "fingerprint");
    json_set(a, "fingerprint", fp ? json_copy((json_t *)fp) : json_string(""));
    json_set(a, "keep_existing", json_bool(true));
    const char *d[] = {"last_success_at", "summary"};
    json_set(a, "drop", da14_arr_str(d, 2));
    json_set(a, "fields", json_null());
    json_t *r = da14_call(ctx, "update_command_sync_entry", a);
    if (da14_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: da14_record_command_sync_success @ plugins/platforms/discord/adapter.py:_record_command_sync_success */
static json_t *da14_record_command_sync_success(da14_ctx_t *ctx, const json_t *args) {
    json_t *t = da14_call(ctx, "time_time", json_object());
    if (da14_is_raise(t)) return t;
    json_t *fields = json_object();
    json_set(fields, "last_success_at", json_copy(t));
    json_set(fields, "summary", da14_copyv(da14_arg(args, "summary")));
    json_free(t);
    json_t *a = json_object();
    json_set(a, "app_id", da14_copyv(da14_arg(args, "app_id")));
    const json_t *fp = da14_arg(args, "fingerprint");
    json_set(a, "fingerprint", fp ? json_copy((json_t *)fp) : json_string(""));
    json_set(a, "keep_existing", json_bool(false));
    json_set(a, "drop", json_null());
    json_set(a, "fields", fields);
    json_t *r = da14_call(ctx, "update_command_sync_entry", a);
    if (da14_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: da14_missed_message_backfill_enabled @ plugins/platforms/discord/adapter.py:_missed_message_backfill_enabled */
static json_t *da14_missed_message_backfill_enabled(da14_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *cfg = da14_state_get(ctx, "config");
    const json_t *extra = da14_getattr(cfg, "extra");
    const json_t *configured = da14_getattr(extra, "missed_message_backfill");
    if (configured && configured->type == JSON_OBJECT && da14_has(configured, "enabled")) {
        const json_t *value = json_obj_get(configured, "enabled");
        if (value && value->type == JSON_STRING) return json_bool(da14_yes(value->str_val));
        return json_bool(da14_truthy(value));
    }
    json_t *a = json_object();
    json_set(a, "key", json_string("DISCORD_MISSED_MESSAGE_BACKFILL"));
    json_set(a, "default", json_string("false"));
    json_t *r = da14_call(ctx, "scoped_gate_env", a);
    if (da14_is_raise(r)) return r;
    bool yes = da14_yes(da14_str(r));
    json_free(r);
    return json_bool(yes);
}

/* PoP: da14_record_discord_message_seen @ plugins/platforms/discord/adapter.py:_record_discord_message_seen */
static json_t *da14_record_discord_message_seen(da14_ctx_t *ctx, const json_t *args) {
    json_t *b = da14_call(ctx, "backfill_enabled", json_object());
    if (da14_is_raise(b)) return b;
    bool enabled = da14_truthy(b); json_free(b);
    if (!enabled) return json_null();

    const json_t *msg = da14_arg(args, "message");
    const char *status = da14_arg_str(args, "status", "discovered");
    char *mid = da14_id_str(msg, "id");
    if (mid[0] == '\0') { free(mid); return json_null(); }

    json_t *a = json_object();
    json_set(a, "message", da14_copyv(msg));
    json_t *ch = da14_call(ctx, "message_channel_ids", a);
    if (da14_is_raise(ch)) { free(mid); return ch; }
    if (!ch || ch->type != JSON_ARRAY || ch->c.count < 3) {
        free(mid); return da14_err("ValueError", "not enough values to unpack (expected 3, got 0)");
    }
    json_t *p0 = json_copy(ch->c.items[0]);
    json_t *p1 = json_copy(ch->c.items[1]);
    json_t *p2 = json_copy(ch->c.items[2]);
    json_free(ch);

    char *aid = da14_id_str(da14_getattr(msg, "author"), "id");
    const json_t *created = da14_getattr(msg, "created_at");
    json_t *created_text = json_null();
    if (created && created->type == JSON_OBJECT && da14_has(created, "__stamp__"))
        created_text = json_copy(json_obj_get(created, "__stamp__"));

    json_t *now = da14_call(ctx, "utc_now_iso", json_object());
    if (da14_is_raise(now)) { free(mid); free(aid); return now; }
    json_t *w = da14_with_db(ctx);
    if (da14_is_raise(w)) { free(mid); free(aid); return w; }
    json_free(w);

    json_t *p = json_array(); json_append(p, json_string(mid));
    json_t *r = da14_exec(ctx, SQL_SEL_STATUS, p);
    if (da14_is_raise(r)) { free(mid); free(aid); return r; }
    json_free(r);
    json_t *row = da14_fetchone(ctx);
    if (da14_is_raise(row)) { free(mid); free(aid); return row; }
    bool keep = (row && row->type == JSON_ARRAY && row->c.count > 0) &&
                ((da14_str(row->c.items[0]) && strcmp(da14_str(row->c.items[0]), "responded") == 0) ||
                 strcmp(status, "discovered") == 0);
    json_t *final_status = keep ? json_copy(row->c.items[0]) : json_string(status);
    json_t *updated = json_null();
    if (keep && row->c.count > 1 && da14_truthy(row->c.items[1]))
        updated = json_copy(row->c.items[1]);
    else
        updated = json_copy(now);
    json_free(row);
    json_free(now);

    json_t *pa = json_array();
    json_append(pa, json_string(mid));
    json_append(pa, p0); json_append(pa, p1); json_append(pa, p2);
    json_append(pa, json_string(aid));
    json_append(pa, created_text);
    json_append(pa, final_status);
    json_append(pa, json_copy(updated));
    json_append(pa, json_copy(final_status));
    r = da14_exec(ctx, SQL_INS_SEEN, pa);
    if (da14_is_raise(r)) { free(mid); free(aid); return r; }
    json_free(r);
    free(mid); free(aid);
    return json_null();
}

/* PoP: da14_record_recovery_attempt @ plugins/platforms/discord/adapter.py:_record_recovery_attempt */
static json_t *da14_record_recovery_attempt(da14_ctx_t *ctx, const json_t *args) {
    json_t *b = da14_call(ctx, "backfill_enabled", json_object());
    if (da14_is_raise(b)) return b;
    bool enabled = da14_truthy(b); json_free(b);
    if (!enabled) return json_null();
    const json_t *msg = da14_arg(args, "message");
    const char *status = da14_arg_str(args, "status", "queued");
    json_t *a = json_object();
    json_set(a, "message", da14_copyv(msg));
    json_set(a, "status", json_string(status));
    json_t *r = da14_call(ctx, "record_discord_message_seen", a);
    if (da14_is_raise(r)) return r;
    json_free(r);
    char *mid = da14_id_str(msg, "id");
    if (mid[0] == '\0') { free(mid); return json_null(); }
    json_t *now = da14_call(ctx, "utc_now_iso", json_object());
    if (da14_is_raise(now)) { free(mid); return now; }
    json_t *w = da14_with_db(ctx);
    if (da14_is_raise(w)) { free(mid); return w; }
    json_free(w);
    json_t *p = json_array();
    json_append(p, json_string(status));
    json_append(p, json_number(strcmp(status, "queued") == 0 ? 1.0 : 0.0));
    json_append(p, json_copy(now));
    json_append(p, da14_copyv(da14_arg(args, "error")));
    json_append(p, json_copy(now));
    json_append(p, json_string(mid));
    r = da14_exec(ctx, SQL_UPD_ATTEMPT, p);
    if (da14_is_raise(r)) { free(mid); return r; }
    json_free(r);
    free(mid);
    return json_null();
}

/* PoP: da14_record_discord_processing_start @ plugins/platforms/discord/adapter.py:_record_discord_processing_start */
static json_t *da14_record_discord_processing_start(da14_ctx_t *ctx, const json_t *args) {
    json_t *b = da14_call(ctx, "backfill_enabled", json_object());
    if (da14_is_raise(b)) return b;
    bool enabled = da14_truthy(b); json_free(b);
    if (!enabled) return json_null();
    const json_t *msg = da14_arg(args, "raw_message");
    json_t *a = json_object();
    json_set(a, "message", da14_copyv(msg));
    json_set(a, "status", json_string("processing"));
    json_t *r = da14_call(ctx, "record_discord_message_seen", a);
    if (da14_is_raise(r)) return r;
    json_free(r);
    char *mid = da14_id_str(msg, "id");
    if (mid[0] == '\0') {
        const char *ev = da14_arg_str(args, "message_id", "");
        free(mid); mid = da14_strdup(ev);
    }
    if (mid[0] == '\0') { free(mid); return json_null(); }
    json_t *now = da14_call(ctx, "utc_now_iso", json_object());
    if (da14_is_raise(now)) { free(mid); return now; }
    json_t *w = da14_with_db(ctx);
    if (da14_is_raise(w)) { free(mid); return w; }
    json_free(w);
    json_t *p = json_array();
    json_append(p, json_bool(da14_truthy(da14_arg(args, "emoji_ack")) ? 1 : 0));
    json_append(p, json_copy(now));
    json_append(p, json_string(mid));
    r = da14_exec(ctx, SQL_UPD_START, p);
    if (da14_is_raise(r)) { free(mid); return r; }
    json_free(r);
    free(mid);
    return json_null();
}

/* PoP: da14_record_discord_processing_complete @ plugins/platforms/discord/adapter.py:_record_discord_processing_complete */
static json_t *da14_record_discord_processing_complete(da14_ctx_t *ctx, const json_t *args) {
    json_t *b = da14_call(ctx, "backfill_enabled", json_object());
    if (da14_is_raise(b)) return b;
    bool enabled = da14_truthy(b); json_free(b);
    if (!enabled) return json_null();
    const json_t *msg = da14_arg(args, "raw_message");
    char *mid = da14_id_str(msg, "id");
    if (mid[0] == '\0') {
        const char *ev = da14_arg_str(args, "message_id", "");
        free(mid); mid = da14_strdup(ev);
    }
    if (mid[0] == '\0') { free(mid); return json_null(); }
    json_t *now = da14_call(ctx, "utc_now_iso", json_object());
    if (da14_is_raise(now)) { free(mid); return now; }
    const char *outcome = da14_arg_str(args, "outcome", "success");
    json_t *w = da14_with_db(ctx);
    if (da14_is_raise(w)) { free(mid); return w; }
    json_free(w);
    json_t *p = json_array();
    if (strcmp(outcome, "success") == 0) {
        json_append(p, json_copy(now));
        json_append(p, json_string(mid));
        json_t *r = da14_exec(ctx, SQL_UPD_SUCCESS, p);
        if (da14_is_raise(r)) { free(mid); return r; }
        json_free(r);
    } else {
        json_append(p, json_string(strcmp(outcome, "cancelled") == 0 ? "cancelled" : "failed"));
        json_append(p, json_copy(now));
        json_append(p, json_string(mid));
        json_t *r = da14_exec(ctx, SQL_UPD_CASE, p);
        if (da14_is_raise(r)) { free(mid); return r; }
        json_free(r);
    }
    free(mid);
    return json_null();
}

/* PoP: da14_record_discord_response @ plugins/platforms/discord/adapter.py:_record_discord_response */
static json_t *da14_record_discord_response(da14_ctx_t *ctx, const json_t *args) {
    json_t *b = da14_call(ctx, "backfill_enabled", json_object());
    if (da14_is_raise(b)) return b;
    bool enabled = da14_truthy(b); json_free(b);
    const json_t *reply = da14_arg(args, "reply_to");
    if (!enabled || da14_is_null(reply)) return json_null();
    const char *reply_to = da14_str(reply) ? da14_str(reply) : "";
    if (reply_to[0] == '\0') return json_null();
    json_t *now = da14_call(ctx, "utc_now_iso", json_object());
    if (da14_is_raise(now)) return now;
    bool completed = da14_truthy(da14_arg(args, "final")) &&
                     da14_truthy(da14_arg(args, "success"));
    const json_t *rmid = da14_arg(args, "result_message_id");
    json_t *w = da14_with_db(ctx);
    if (da14_is_raise(w)) return w;
    json_free(w);
    json_t *p = json_array();
    json_append(p, json_string(reply_to));
    json_append(p, json_string(completed ? "responded" : "failed"));
    json_append(p, json_number(completed ? 1.0 : 0.0));
    json_append(p, da14_copyv(rmid));
    json_append(p, json_copy(now));
    json_append(p, json_number(completed ? 1.0 : 0.0));
    json_append(p, json_number(completed ? 1.0 : 0.0));
    json_append(p, json_number(completed ? 1.0 : 0.0));
    json_append(p, da14_copyv(rmid));
    json_append(p, json_copy(now));
    json_t *r = da14_exec(ctx, SQL_INS_RESP, p);
    if (da14_is_raise(r)) return r;
    json_free(r);
    if (completed) {
        w = da14_with_db(ctx);
        if (da14_is_raise(w)) return w;
        json_free(w);
        p = json_array(); json_append(p, json_string(reply_to));
        r = da14_exec(ctx, SQL_SEL_CHANNEL, p);
        if (da14_is_raise(r)) return r;
        json_free(r);
        json_t *row = da14_fetchone(ctx);
        if (da14_is_raise(row)) return row;
        bool have = row && row->type == JSON_ARRAY && row->c.count > 0 && da14_truthy(row->c.items[0]);
        char *chan = have ? da14_py_str(row->c.items[0]) : NULL;
        json_free(row);
        if (chan && chan[0] != '\0') {
            json_t *a = json_object();
            json_set(a, "channel_id", json_string(chan));
            json_set(a, "reply_to", json_string(reply_to));
            r = da14_call(ctx, "advance_recovery_cursor", a);
            if (da14_is_raise(r)) { free(chan); return r; }
            json_free(r);
        }
        free(chan);
    }
    return json_null();
}

/* PoP: da14_record_response_async @ plugins/platforms/discord/adapter.py:_record_response_async */
static json_t *da14_record_response_async(da14_ctx_t *ctx, const json_t *args) {
    const json_t *reply = da14_arg(args, "reply_to");
    const json_t *meta = da14_arg(args, "metadata");
    json_t *ledger = NULL;
    if (da14_truthy(reply)) ledger = json_copy((json_t *)reply);
    else {
        const json_t *m = da14_getattr(meta, "reply_to_message_id");
        ledger = m ? json_copy((json_t *)m) : json_null();
    }
    json_t *result = json_object();
    json_set(result, "success", json_bool(da14_truthy(da14_arg(args, "success"))));
    json_set(result, "message_id", da14_copyv(da14_arg(args, "result_message_id")));
    bool final = da14_truthy(da14_arg(args, "final"));
    const char *content = da14_arg_str(args, "content", "");

    json_t *kwargs = json_object();
    json_set(kwargs, "reply_to", json_copy(ledger));
    json_set(kwargs, "result", json_copy(result));
    json_set(kwargs, "content", json_string(content));
    json_set(kwargs, "final", json_bool(final));
    json_t *a = json_object();
    json_set(a, "func", json_string("<func>"));
    json_set(a, "args", json_array());
    json_set(a, "kwargs", json_copy(kwargs));
    json_t *r = da14_call(ctx, "to_thread", a);
    if (da14_is_raise(r)) return r;
    json_free(r);
    r = da14_call(ctx, "record_discord_response", kwargs);
    if (da14_is_raise(r)) return r;
    json_free(r);
    return result;
}

/* PoP: da14_record_recovery_scan_start @ plugins/platforms/discord/adapter.py:_record_recovery_scan_start */
static json_t *da14_record_recovery_scan_start(da14_ctx_t *ctx, const json_t *args) {
    json_t *t = da14_call(ctx, "time_time", json_object());
    if (da14_is_raise(t)) return t;
    json_t *pid = da14_call(ctx, "os_getpid", json_object());
    if (da14_is_raise(pid)) { json_free(t); return pid; }
    char *sid = da14_fmt("%.0f-%.0f", floor(da14_num(t) * 1000.0), da14_num(pid));
    json_free(t); json_free(pid);
    json_t *now = da14_call(ctx, "utc_now_iso", json_object());
    if (da14_is_raise(now)) { free(sid); return now; }
    json_t *w = da14_with_db(ctx);
    if (da14_is_raise(w)) { free(sid); return w; }
    json_free(w);
    json_t *sorted = da14_sorted_strings(da14_arg(args, "channels"));
    json_t *a = json_object(); json_set(a, "value", json_copy(sorted));
    json_t *jd = da14_call(ctx, "json_dumps", a);
    if (da14_is_raise(jd)) { free(sid); return jd; }
    json_t *win = da14_call(ctx, "backfill_window", json_object());
    if (da14_is_raise(win)) { free(sid); return win; }
    json_t *lim = da14_call(ctx, "backfill_limit", json_object());
    if (da14_is_raise(lim)) { free(sid); return lim; }
    json_t *p = json_array();
    json_append(p, json_string(sid));
    json_append(p, json_copy(now));
    json_append(p, json_string("running"));
    json_append(p, json_copy(jd));
    json_append(p, json_copy(win));
    json_append(p, json_copy(lim));
    json_t *r = da14_exec(ctx, SQL_INS_SCAN, p);
    if (da14_is_raise(r)) { free(sid); return r; }
    json_free(r);
    return json_string(sid);
}

/* PoP: da14_record_recovery_scan_complete @ plugins/platforms/discord/adapter.py:_record_recovery_scan_complete */
static json_t *da14_record_recovery_scan_complete(da14_ctx_t *ctx, const json_t *args) {
    json_t *now = da14_call(ctx, "utc_now_iso", json_object());
    if (da14_is_raise(now)) return now;
    json_t *w = da14_with_db(ctx);
    if (da14_is_raise(w)) return w;
    json_free(w);
    json_t *p = json_array();
    json_append(p, json_copy(now));
    json_append(p, json_string(da14_arg_str(args, "status", "completed")));
    json_append(p, json_number(da14_num(da14_arg(args, "scanned"))));
    json_append(p, json_number(da14_num(da14_arg(args, "missed"))));
    json_append(p, json_number(da14_num(da14_arg(args, "dispatched"))));
    json_append(p, da14_copyv(da14_arg(args, "error")));
    json_append(p, json_string(da14_arg_str(args, "scan_id", "")));
    json_t *r = da14_exec(ctx, SQL_UPD_SCAN, p);
    if (da14_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: da14_refresh_skill_catalog_state @ plugins/platforms/discord/adapter.py:_refresh_skill_catalog_state */
static json_t *da14_refresh_skill_catalog_state(da14_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *raw_reserved = da14_sorted_strings(da14_state_get(ctx, "_skill_group_reserved_names"));
    json_t *a = json_object(); json_set(a, "reserved_names", json_copy(raw_reserved));
    json_t *r = da14_call(ctx, "skill_commands_by_category", a);
    if (da14_is_raise(r)) return r;
    if (!r || r->type != JSON_ARRAY || r->c.count < 3)
        return da14_err("ValueError", "not enough values to unpack (expected 3, got 0)");
    json_t *categories = r->c.items[0];
    json_t *uncat = r->c.items[1];
    json_t *hidden = r->c.items[2];
    json_t *entries = json_array();
    if (uncat && uncat->type == JSON_ARRAY)
        for (size_t i = 0; i < uncat->c.count; i++) json_append(entries, json_copy(uncat->c.items[i]));
    if (categories && categories->type == JSON_OBJECT)
        for (size_t i = 0; i < categories->c.count; i++) {
            json_t *lst = categories->c.items[i];
            if (lst && lst->type == JSON_ARRAY)
                for (size_t j = 0; j < lst->c.count; j++) json_append(entries, json_copy(lst->c.items[j]));
        }
    /* sort by t[0] */
    for (size_t i = 0; i < entries->c.count; i++)
        for (size_t j = i + 1; j < entries->c.count; j++) {
            const json_t *ei = entries->c.items[i], *ej = entries->c.items[j];
            const char *si = (ei->type == JSON_ARRAY && ei->c.count) ? da14_str(ei->c.items[0]) : NULL;
            const char *sj = (ej->type == JSON_ARRAY && ej->c.count) ? da14_str(ej->c.items[0]) : NULL;
            if ((si && sj && strcmp(si, sj) > 0) || (si && !sj)) {
                json_t *t = entries->c.items[i]; entries->c.items[i] = entries->c.items[j]; entries->c.items[j] = t;
            }
        }
    json_t *lookup = json_object();
    for (size_t i = 0; i < entries->c.count; i++) {
        json_t *e = entries->c.items[i];
        if (e->type == JSON_ARRAY && e->c.count >= 3) {
            json_t *pair = json_array();
            json_append(pair, json_copy(e->c.items[1]));
            json_append(pair, json_copy(e->c.items[2]));
            json_set(lookup, da14_str(e->c.items[0]) ? da14_str(e->c.items[0]) : "", pair);
        }
    }
    da14_state_set(ctx, "_skill_entries", json_copy(entries));
    da14_state_set(ctx, "_skill_lookup", lookup);
    da14_state_set(ctx, "_skill_group_hidden_count", json_copy(hidden));
    da14_state_set(ctx, "_skill_group_reserved_names", da14_sorted_strings(raw_reserved));
    return json_null();
}

/* helper: fire t_discord seam; returns owned ret */
static json_t *da14_t_discord(da14_ctx_t *ctx, const char *key) {
    json_t *a = json_object();
    json_set(a, "key", json_string(key));
    json_set(a, "limit", json_number(DA14_TEXT_LIMIT));
    return da14_call(ctx, "t_discord", a);
}

/* PoP: da14_register_skill_group @ plugins/platforms/discord/adapter.py:_register_skill_group */
static json_t *da14_register_skill_group(da14_ctx_t *ctx, const json_t *args) {
    const char *name = da14_arg_str(args, "name", "bot");
    json_t *tc = da14_call(ctx, "tree_get_commands", json_object());
    json_t *existing = NULL;
    if (da14_is_raise(tc)) {
        if (!da14_is_exception(da14_raise_kind(tc))) return tc;
        existing = json_array();
    } else {
        existing = (tc && tc->type == JSON_ARRAY) ? json_copy(tc) : json_array();
    }
    json_t *reserved = da14_sorted_strings(existing);
    da14_state_set(ctx, "_skill_entries", json_array());
    da14_state_set(ctx, "_skill_lookup", json_object());
    da14_state_set(ctx, "_skill_group_reserved_names", json_copy(reserved));

    json_t *rr = da14_call(ctx, "refresh_skill_catalog_state", json_object());
    json_t *fail = NULL;
    if (da14_is_raise(rr)) {
        if (!da14_is_exception(da14_raise_kind(rr))) return rr;
        fail = rr;
    } else if (rr && rr->type == JSON_OBJECT && da14_has(rr, "entries")) {
        json_t *en = json_obj_get(rr, "entries");
        json_t *hc = json_obj_get(rr, "hidden");
        da14_state_set(ctx, "_skill_entries", json_copy(en));
        da14_state_set(ctx, "_skill_group_hidden_count", hc ? json_copy(hc) : json_number(0.0));
    }
    json_t *entries = (json_t *)da14_state_get(ctx, "_skill_entries");
    size_t nentries = (entries && entries->type == JSON_ARRAY) ? entries->c.count : 0;
    if (!fail && nentries > 0) {
        json_t *d;
        d = da14_t_discord(ctx, "platform.discord.command.skill.arg_name");
        if (da14_is_raise(d)) { fail = d; }
        if (!fail) { d = da14_t_discord(ctx, "platform.discord.command.skill.arg_args");
                     if (da14_is_raise(d)) fail = d; }
        if (!fail) { d = da14_t_discord(ctx, "platform.discord.command.skill.description");
                     if (da14_is_raise(d)) fail = d; }
        if (!fail) {
            json_t *a = json_object();
            json_set(a, "name", json_string("skill"));
            json_set(a, "description", json_copy(d));
            json_set(a, "callback", json_string("_skill_handler"));
            d = da14_call(ctx, "app_command_ctor", a);
            if (da14_is_raise(d)) fail = d;
        }
        if (!fail) {
            json_t *a = json_object();
            json_t *cmd = json_object(); json_set(cmd, "name", json_string("skill"));
            json_set(a, "command", cmd);
            d = da14_call(ctx, "tree_add_command", a);
            if (da14_is_raise(d)) fail = d;
        }
        if (!fail) {
            json_t *ar = json_array();
            json_append(ar, json_string("[%s] Registered /skill command with %d skill(s) via autocomplete"));
            json_append(ar, json_string(name));
            json_append(ar, json_number((double)nentries));
            d = da14_call(ctx, "logger_info", da14_logarr(ar));
            if (da14_is_raise(d)) fail = d;
        }
        if (!fail) {
            const json_t *hc = da14_state_get(ctx, "_skill_group_hidden_count");
            if (da14_truthy(hc)) {
                json_t *ar = json_array();
                json_append(ar, json_string("[%s] %d skill(s) filtered out of /skill (name clamp / reserved)"));
                json_append(ar, json_string(name));
                json_append(ar, json_copy((json_t *)hc));
                d = da14_call(ctx, "logger_info", da14_logarr(ar));
                if (da14_is_raise(d)) fail = d;
            }
        }
    }
    if (fail) {
        if (!da14_is_exception(da14_raise_kind(fail))) return fail;
        json_t *ar = json_array();
        json_append(ar, json_string("[%s] Failed to register /skill command: %s"));
        json_append(ar, json_string(name));
        json_append(ar, json_copy(fail));
        json_t *r = da14_call(ctx, "logger_warning", da14_logkw(ar));
        if (da14_is_raise(r)) return r;
    }
    return json_null();
}

/* PoP: da14_register_thread_slash @ plugins/platforms/discord/adapter.py:_register_thread_slash */
static json_t *da14_register_thread_slash(da14_ctx_t *ctx, const json_t *args) {
    const char *nm = da14_arg_str(args, "name", "thread");
    const char *ds = da14_arg_str(args, "description", "desc");
    json_t *a = json_object();
    json_set(a, "name", json_string(nm));
    json_set(a, "description", json_string(ds));
    json_t *r = da14_call(ctx, "tree_command", a);
    if (da14_is_raise(r)) return r;
    const char *keys[] = {
        "platform.discord.command.thread.arg_name",
        "platform.discord.command.thread.arg_message",
        "platform.discord.command.thread.arg_auto_archive",
    };
    for (size_t i = 0; i < 3; i++) {
        r = da14_t_discord(ctx, keys[i]);
        if (da14_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* helper for _register_slash_commands auto_register */
static json_t *da14_auto_register(da14_ctx_t *ctx, json_t *already, int *dropped,
                                  const char *name, const char *description, const char *args_hint) {
    char *low = da14_lower(name);
    char dn[33];
    strncpy(dn, low, 32); dn[32] = '\0';
    if (da14_arr_has(already, dn)) { free(low); return NULL; }
    if (json_len(already) >= DA14_MAX_APP_COMMANDS - 1) { (*dropped)++; free(low); return NULL; }

    json_t *args_val = json_array();
    bool hint = (args_hint && args_hint[0] != '\0');
    if (hint) {
        json_t *a = json_object();
        json_set(a, "key", json_string("platform.discord.command.auto_args_description"));
        json_set(a, "limit", json_number(DA14_TEXT_LIMIT));
        json_set(a, "args_hint", json_string(args_hint));
        json_t *td = da14_call(ctx, "t_discord", a);
        if (da14_is_raise(td)) { free(low); return td; }
        json_t *spec = json_array();
        json_append(spec, json_string("args"));
        json_append(spec, json_string("<callable>"));
        json_append(spec, json_string(""));
        json_append(spec, json_copy(td));
        json_append(spec, json_null());
        json_append(args_val, spec);
        json_free(td);
    }
    char tpl[512];
    if (hint) snprintf(tpl, sizeof tpl, "/%s {args}", name);
    else snprintf(tpl, sizeof tpl, "/%s", name);

    json_t *desc_text = NULL;
    if (description && description[0] != '\0') desc_text = json_string(description);
    else {
        json_t *a = json_object();
        json_set(a, "key", json_string("platform.discord.command.auto_description_fallback"));
        json_set(a, "name", json_string(name));
        desc_text = da14_call(ctx, "t", a);
        if (da14_is_raise(desc_text)) { free(low); return desc_text; }
    }
    json_t *ta = json_object();
    json_set(ta, "text", json_copy(desc_text));
    json_set(ta, "limit", json_number(DA14_TEXT_LIMIT));
    json_t *tr = da14_call(ctx, "truncate_text", ta);
    if (da14_is_raise(tr)) { free(low); return tr; }

    json_t *sa = json_object();
    json_set(sa, "name", json_string(name));
    json_set(sa, "args", args_val);
    json_set(sa, "template", json_string(tpl));
    json_set(sa, "followup", json_null());
    json_set(sa, "strip", json_bool(hint));
    json_set(sa, "prefix", json_string("auto_slash_"));
    json_t *proxy = da14_call(ctx, "slash_proxy", sa);
    if (da14_is_raise(proxy)) { free(low); return proxy; }
    json_free(proxy);

    json_t *ca = json_object();
    json_set(ca, "name", json_string(dn));
    json_set(ca, "description", json_copy(tr));
    json_set(ca, "callback", json_string("<callback>"));
    json_t *ctor = da14_call(ctx, "app_command_ctor", ca);
    if (da14_is_raise(ctor)) { free(low); return ctor; }
    json_free(ctor);

    json_t *aa = json_object();
    json_t *cmd = json_object(); json_set(cmd, "name", json_string(dn));
    json_set(aa, "command", cmd);
    json_t *add = da14_call(ctx, "tree_add_command", aa);
    if (da14_is_raise(add)) { free(low); return add; }
    json_free(add);
    json_append(already, json_string(dn));
    free(low);
    return NULL;
}
/* PoP: da14_register_slash_commands @ plugins/platforms/discord/adapter.py:_register_slash_commands */
static json_t *da14_register_slash_commands(da14_ctx_t *ctx, const json_t *args) {
    const char *name = da14_arg_str(args, "name", "bot");
    const json_t *client = da14_state_get(ctx, "_client");
    if (da14_is_null(client)) return json_null();

    json_t *nc = da14_call(ctx, "native_slash_commands", json_object());
    if (da14_is_raise(nc)) return nc;
    if (nc && nc->type == JSON_ARRAY)
        for (size_t i = 0; i < nc->c.count; i++) {
            json_t *it = nc->c.items[i];
            if (!it || it->type != JSON_ARRAY || it->c.count < 5) continue;
            const char *nm = da14_str(it->c.items[0]) ? da14_str(it->c.items[0]) : "";
            const char *ds = da14_str(it->c.items[1]) ? da14_str(it->c.items[1]) : "";
            const json_t *tpl = it->c.items[3];
            if (da14_is_null(tpl)) {
                json_t *a = json_object();
                json_set(a, "tree", json_string("<tree>"));
                json_set(a, "name", json_string(nm));
                json_set(a, "description", json_string(ds));
                json_t *r = da14_call(ctx, "register_thread_slash", a);
                if (da14_is_raise(r)) return r;
                json_free(r);
                continue;
            }
            json_t *a = json_object();
            json_set(a, "name", json_string(nm));
            json_set(a, "description", json_string(ds));
            json_t *r = da14_call(ctx, "tree_command", a);
            if (da14_is_raise(r)) return r;
            json_free(r);
            json_t *sa = json_object();
            json_set(sa, "name", json_string(nm));
            json_set(sa, "args", json_copy(it->c.items[2]));
            json_set(sa, "template", json_copy(tpl));
            json_set(sa, "followup", json_copy(it->c.items[4]));
            json_set(sa, "strip", json_bool(strcmp(nm, "insights") != 0));
            json_set(sa, "prefix", json_string(""));
            r = da14_call(ctx, "slash_proxy", sa);
            if (da14_is_raise(r)) return r;
            json_free(r);
        }

    json_t *already = json_array();
    int dropped = 0;
    json_t *fail = NULL;
    json_t *tc = da14_call(ctx, "tree_get_commands", json_object());
    if (da14_is_raise(tc)) {
        if (!da14_is_exception(da14_raise_kind(tc))) return tc;
    } else {
        json_t *srt = da14_sorted_strings(tc);
        json_free(tc);
        json_free(already); already = srt;
    }
    json_t *cfg = da14_call(ctx, "resolve_config_gates", json_object());
    if (da14_is_raise(cfg)) {
        if (!da14_is_exception(da14_raise_kind(cfg))) return cfg;
        fail = cfg;
    }
    json_t *reg = da14_call(ctx, "command_registry", json_object());
    if (da14_is_raise(reg)) {
        if (!da14_is_exception(da14_raise_kind(reg))) return reg;
        fail = reg;
    } else {
            if (reg && reg->type == JSON_ARRAY)
                for (size_t i = 0; i < reg->c.count && !fail; i++) {
                    json_t *d = reg->c.items[i];
                    const char *nm = (d->type == JSON_ARRAY && d->c.count > 0) ? da14_str(d->c.items[0]) : NULL;
                    const char *ds = (d->type == JSON_ARRAY && d->c.count > 1) ? da14_str(d->c.items[1]) : NULL;
                    const char *ah = (d->type == JSON_ARRAY && d->c.count > 2) ? da14_str(d->c.items[2]) : NULL;
                    json_t *g = json_object();
                    json_t *cd = json_object();
                    json_set(cd, "name", json_string(nm ? nm : ""));
                    json_set(cd, "description", json_string(ds ? ds : ""));
                    json_set(cd, "args_hint", json_string(ah ? ah : ""));
                    json_set(g, "cmd_def", cd);
                    json_set(g, "config_overrides", json_copy(cfg));
                    json_t *av = da14_call(ctx, "is_gateway_available", g);
                    if (da14_is_raise(av)) {
                        if (!da14_is_exception(da14_raise_kind(av))) return av;
                        fail = av; break;
                    }
                    bool ok = da14_truthy(av); json_free(av);
                    if (ok) {
                        json_t *e = da14_auto_register(ctx, already, &dropped, nm ? nm : "", ds ? ds : "", ah ? ah : "");
                        if (da14_is_raise(e)) {
                            if (!da14_is_exception(da14_raise_kind(e))) return e;
                            fail = e;
                        }
                    }
                }
            if (!fail) {
                json_t *ar = json_array();
                json_append(ar, json_string("Discord auto-registered %d commands from COMMAND_REGISTRY"));
                json_append(ar, json_number((double)json_len(already)));
                json_t *r = da14_call(ctx, "logger_debug", da14_logarr(ar));
                if (da14_is_raise(r)) {
                    if (!da14_is_exception(da14_raise_kind(r))) return r;
                    fail = r;
                }
            }
        }
    if (fail) {
        json_t *ar = json_array();
        json_append(ar, json_string("Discord auto-register from COMMAND_REGISTRY failed: %s"));
        json_append(ar, json_copy(fail));
        json_t *r = da14_call(ctx, "logger_warning", da14_logkw(ar));
        if (da14_is_raise(r)) return r;
        fail = NULL;
    }
    json_t *pe = da14_call(ctx, "iter_plugin_command_entries", json_object());
    if (da14_is_raise(pe)) {
        if (!da14_is_exception(da14_raise_kind(pe))) return pe;
        json_t *ar = json_array();
        json_append(ar, json_string("Discord auto-register from plugin commands failed: %s"));
        json_append(ar, json_copy(pe));
        json_t *r = da14_call(ctx, "logger_warning", da14_logkw(ar));
        if (da14_is_raise(r)) return r;
    } else if (pe && pe->type == JSON_ARRAY) {
        for (size_t i = 0; i < pe->c.count; i++) {
            json_t *d = pe->c.items[i];
            const char *nm = (d->type == JSON_ARRAY && d->c.count > 0) ? da14_str(d->c.items[0]) : NULL;
            const char *ds = (d->type == JSON_ARRAY && d->c.count > 1) ? da14_str(d->c.items[1]) : NULL;
            const char *ah = (d->type == JSON_ARRAY && d->c.count > 2) ? da14_str(d->c.items[2]) : NULL;
            json_t *e = da14_auto_register(ctx, already, &dropped, nm ? nm : "", ds ? ds : "", ah ? ah : "");
            if (da14_is_raise(e)) {
                if (!da14_is_exception(da14_raise_kind(e))) return e;
                json_t *ar = json_array();
                json_append(ar, json_string("Discord auto-register from plugin commands failed: %s"));
                json_append(ar, json_copy(e));
                json_t *r = da14_call(ctx, "logger_warning", da14_logkw(ar));
                if (da14_is_raise(r)) return r;
                break;
            }
        }
    }

    json_t *a = json_object(); json_set(a, "tree", json_string("<tree>"));
    json_t *r = da14_call(ctx, "register_skill_group", a);
    if (da14_is_raise(r)) return r;
    json_free(r);

    if (dropped) {
        json_t *ar = json_array();
        json_append(ar, json_string("[%s] Reached Discord's limit of %d slash commands; skipped %d lower-priority command(s) to keep the command sync working. Disable slash commands you don't need or trim installed plugins to surface them all."));
        json_append(ar, json_string(name));
        json_append(ar, json_number(DA14_MAX_APP_COMMANDS));
        json_append(ar, json_number((double)dropped));
        r = da14_call(ctx, "logger_warning", da14_logkw(ar));
        if (da14_is_raise(r)) return r;
        json_free(r);
    }
    json_t *ea = json_object();
    json_set(ea, "key", json_string("DISCORD_HIDE_SLASH_COMMANDS"));
    json_set(ea, "default", json_string("false"));
    json_t *env = da14_call(ctx, "scoped_gate_env", ea);
    if (da14_is_raise(env)) return env;
    bool hide = da14_yes(da14_str(env));
    json_free(env);
    if (hide) {
        json_t *a2 = json_object(); json_set(a2, "tree", json_string("<tree>"));
        r = da14_call(ctx, "apply_owner_only_visibility", a2);
        if (da14_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: da14_reject_slash @ plugins/platforms/discord/adapter.py:_reject_slash */
static json_t *da14_reject_slash(da14_ctx_t *ctx, const json_t *args) {
    const json_t *ix = da14_arg(args, "interaction");
    const json_t *user = da14_getattr(ix, "user");
    char *uid, *uname;
    if (!da14_is_null(user)) {
        const json_t *idv = da14_getattr(user, "id");
        uid = da14_py_str(idv ? idv : json_string("?"));
        const json_t *nm = da14_getattr(user, "name");
        uname = da14_py_str(nm ? nm : json_string("?"));
    } else { uid = da14_strdup("?"); uname = da14_strdup("?"); }

    const json_t *chan_id = da14_getattr(ix, "channel_id");
    if (da14_is_null(chan_id)) chan_id = da14_getattr(da14_getattr(ix, "channel"), "id");
    const json_t *guild_id = da14_getattr(ix, "guild_id");
    const char *ctext = da14_arg_str(args, "command_text", "");
    const char *reason = da14_arg_str(args, "reason", "");
    char *cmd0 = da14_strdup(ctext);
    char *sp = strchr(cmd0, ' ');
    if (sp) *sp = '\0';

    json_t *ar = json_array();
    json_append(ar, json_string("[Discord] Unauthorized slash attempt: id=%s channel=%s guild=%s cmd=%r reason=%r"));
    json_append(ar, json_string(uid));
    json_append(ar, da14_copyv(chan_id));
    json_append(ar, da14_copyv(guild_id));
    json_append(ar, json_string(cmd0));
    json_append(ar, json_string(reason));
    json_t *r = da14_call(ctx, "logger_warning", da14_logkw(ar));
    if (da14_is_raise(r)) { free(uid); free(uname); free(cmd0); return r; }

    json_t *u = da14_call(ctx, "unauthorized", json_object());
    if (da14_is_raise(u)) { free(uid); free(uname); free(cmd0); return u; }
    json_t *sa = json_object();
    json_set(sa, "message", json_copy(u));
    json_set(sa, "ephemeral", json_bool(true));
    r = da14_call(ctx, "response_send_message", sa);
    if (da14_is_raise(r)) {
        if (!da14_is_exception(da14_raise_kind(r))) { free(uid); free(uname); free(cmd0); return r; }
        json_t *dr = json_array();
        json_append(dr, json_string("[Discord] Could not send unauthorized ephemeral: %s"));
        json_append(dr, json_copy(r));
        json_t *x = da14_call(ctx, "logger_debug", da14_logarr(dr));
        if (da14_is_raise(x)) { free(uid); free(uname); free(cmd0); return x; }
    }

    json_t *na = json_object();
    json_set(na, "user_name", json_string(uname));
    json_set(na, "user_id", json_string(uid));
    json_set(na, "chan_id", da14_copyv(chan_id));
    json_set(na, "guild_id", da14_copyv(guild_id));
    json_set(na, "command_text", json_string(ctext));
    json_set(na, "reason", json_string(reason));
    r = da14_call(ctx, "notify_unauthorized_slash", na);
    if (da14_is_raise(r)) { free(uid); free(uname); free(cmd0); return r; }
    json_free(r);
    r = da14_call(ctx, "create_task", json_object());
    if (da14_is_raise(r)) {
        if (!da14_is_exception(da14_raise_kind(r))) { free(uid); free(uname); free(cmd0); return r; }
        json_t *dr = json_array();
        json_append(dr, json_string("[Discord] Could not schedule admin notify task: %s"));
        json_append(dr, json_copy(r));
        json_t *x = da14_call(ctx, "logger_debug", da14_logarr(dr));
        if (da14_is_raise(x)) { free(uid); free(uname); free(cmd0); return x; }
    }
    free(uid); free(uname); free(cmd0);
    return json_bool(false);
}
