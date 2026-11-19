/* cron_jobs_flow.c — FLOW + leaf port of cron/jobs.py job-lifecycle flows.
 *
 * Ports the storage/lifecycle decision chains that were still unclaimed in
 * src/cron: self-removal delivery gate, cross-process flock acquire/release
 * (bounded poll loop), cron-dir creation guard, cron-expression validation
 * ordering, ISO-aware parse error paths, telemetry append (bounded recent
 * list + best-effort file write), timezone-migration catch-up recording,
 * atomic marker write/read, ticker writer liveness, shrink-merge disk
 * recovery, quiet unlink, fsynced payload staging (BaseException cleanup),
 * the _with_job locked CRUD callback fence, past-one-shot rejection, the
 * pinned-model update rewrite and compare-and-refresh claim heartbeat.
 *
 * Seam model (mirrors port_scheduler_flow.c): every live module-level I/O
 * call, claimed sibling, or inner-imported dependency goes through
 * cjf_io_t.call(io, name, args). The dispatcher appends `name` to io->trace
 * (ordered decision sequence, NAME-ONLY tokens exactly like scheduler_flow),
 * returns the scripted result, applies "state" effects onto io->state, and
 * models a raised exception with {"__raise__": "<Type>", "message": "..."}.
 * io->state is a json_t OBJECT mirroring the module's mutable globals
 * (recent telemetry lists, counters, load_stamp, platform, marker).
 *
 * Documented projections (deterministic subset):
 *  - datetimes are ISO strings; .isoformat() is identity. _hermes_now returns
 *    the scripted ISO string directly (the oracle wraps it in a double whose
 *    .isoformat() yields the same string).
 *  - paths are POSIX strings; Path `/` join and `.parent` are pure helpers
 *    (cjf_path_join / cjf_dirname), NOT seams. The oracle's store/path doubles
 *    use PurePosixPath semantics so Windows host separators never leak.
 *  - lock_fd is an int fd; .fileno() is identity, .close() is the "close" seam.
 *  - json.dump (indented, chunked writes) is modeled as ONE "json_dump" seam;
 *    the file double's write is silent. json.dumps stays real but its text is
 *    not trace-compared (name-only trace).
 *  - _jobs_lock is a "jobs_lock.enter"/"jobs_lock.exit" seam pair; the fn
 *    callback of _with_job is the "fn" seam.
 *  - BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit)
 *    pass through every except-Exception / suppress(OSError) site.
 *
 * Ownership: seam results are caller-owned (json_free). io->trace/io->state
 * belong to the harness. Args passed to seam() are freed by seam(). Out-param
 * json objects (updates/claim/jobs/recent) are borrowed and mutated in place.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── live module constants (byte-pinned to cron/jobs.py) ───────────────── */
#define CJF_TELEMETRY_RECENT_HISTORY 20
#define CJF_ONESHOT_GRACE_SECONDS 120
#define CJF_JOBS_LOCK_TIMEOUT_SECONDS 30.0
#define CJF_FLOCK_POLL_INTERVAL 0.1

/* ── seam dispatcher ───────────────────────────────────────────────────── */
typedef struct cjf_io_t {
    json_t *(*call)(struct cjf_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of strings: ordered decision sequence */
    json_t *state;   /* JSON object: module mutable globals */
    void   *ctx;     /* harness backend (scripted results) */
} cjf_io_t;

static void cjf_trace(cjf_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}

static json_t *cjf_call(cjf_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}

static bool cjf_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

/* Call a seam, record its name, free args, return owned result (never NULL). */
static json_t *cjf_seam(cjf_io_t *io, const char *name, json_t *args) {
    cjf_trace(io, name);
    json_t *r = cjf_call(io, name, args);
    if (args) json_free(args);
    return r ? r : json_null();
}

static json_t *cjf_args(void) { return json_object(); }

static void cjf_log(cjf_io_t *io, const char *level) {
    char tok[64];
    snprintf(tok, sizeof tok, "logger.%s", level);
    json_t *r = cjf_seam(io, tok, cjf_args());
    json_free(r);
}

static json_t *cjf_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type ? type : "Exception"));
    json_set(e, "message", json_string(msg ? msg : ""));
    return e;
}

static const char *cjf_exc_kind(const json_t *e) {
    return json_get_str(e, "__raise__", "");
}

/* except Exception does NOT catch these; they propagate through every
 * catch-equivalent branch (contextlib.suppress included). */
static bool cjf_base_exc(const json_t *e) {
    const char *k = cjf_exc_kind(e);
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0;
}

/* OSError family: contextlib.suppress(OSError) / except OSError catch these. */
static bool cjf_oserror_kind(const char *k) {
    static const char *fam[] = {
        "OSError", "FileNotFoundError", "PermissionError", "FileExistsError",
        "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "BlockingIOError", "ChildProcessError", "ConnectionError",
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", "TimeoutError", "ProcessLookupError",
    };
    for (size_t i = 0; i < sizeof fam / sizeof fam[0]; i++)
        if (strcmp(k, fam[i]) == 0) return true;
    return false;
}

/* ── Python-faithful value helpers ─────────────────────────────────────── */
static bool cjf_truthy(const json_t *v) {
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

static char *cjf_strdup(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *o = malloc(n + 1);
    if (o) memcpy(o, p, n + 1);
    return o;
}

static int cjf_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() (ASCII whitespace subset; fixtures are ASCII). */
static char *cjf_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && cjf_is_ws((unsigned char)p[a])) a++;
    while (b > a && cjf_is_ws((unsigned char)p[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, p + a, b - a);
    o[b - a] = '\0';
    return o;
}

/* Python type name for a JSON value (AttributeError messages). */
static const char *cjf_py_typename(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* Shortest round-trip float repr (CPython repr algorithm subset). */
static void cjf_fmt_float(char *buf, size_t cap, double d) {
    if (d == floor(d) && fabs(d) < 1e16) {
        snprintf(buf, cap, "%.0f.0", d);
        return;
    }
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
}

/* Python str() of a JSON scalar (message formatting). Containers are NOT
 * repr'd here — fixtures keep formatted values scalar (documented). */
static char *cjf_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return cjf_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return cjf_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e16)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else
                cjf_fmt_float(buf, sizeof buf, v->num_val);
            return cjf_strdup(buf);
        case JSON_STRING: return cjf_strdup(v->str_val ? v->str_val : "");
        default: return cjf_strdup("");
    }
}

/* str(KeyError(k)) is repr(k); every other class uses str(exc) verbatim. */
static char *cjf_exc_str(const json_t *e) {
    const char *msg = json_get_str(e, "message", "");
    if (strcmp(cjf_exc_kind(e), "KeyError") == 0) {
        size_t n = strlen(msg);
        char *o = malloc(n + 3);
        if (!o) return cjf_strdup(msg);
        o[0] = '\'';
        memcpy(o + 1, msg, n);
        o[n + 1] = '\'';
        o[n + 2] = '\0';
        return o;
    }
    return cjf_strdup(msg);
}

/* Python == for JSON scalars (bool is an int subclass: True == 1). */
static bool cjf_json_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    bool an = a->type == JSON_NUMBER || a->type == JSON_BOOL;
    bool bn = b->type == JSON_NUMBER || b->type == JSON_BOOL;
    if (an && bn) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL: return true;
        case JSON_STRING:
            return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!cjf_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !cjf_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return false;
    }
}

/* PurePosixPath(base) / name — absolute name resets; no seam (internal). */
static char *cjf_path_join(const char *base, const char *name) {
    const char *b = base ? base : "";
    const char *n = name ? name : "";
    if (n[0] == '/') return cjf_strdup(n);
    size_t bl = strlen(b);
    while (bl > 1 && b[bl - 1] == '/') bl--;
    if (bl == 0) return cjf_strdup(n);
    size_t nl = strlen(n);
    char *o = malloc(bl + 1 + nl + 1);
    if (!o) return NULL;
    memcpy(o, b, bl);
    o[bl] = '/';
    memcpy(o + bl + 1, n, nl + 1);
    return o;
}

/* PurePosixPath(p).parent as a POSIX string. */
static char *cjf_dirname(const char *p) {
    const char *s = p ? p : "";
    size_t n = strlen(s);
    while (n > 1 && s[n - 1] == '/') n--;
    if (n == 0) return cjf_strdup(".");
    if (n == 1 && s[0] == '/') return cjf_strdup("/");
    const char *slash = NULL;
    for (size_t i = n; i > 0; i--)
        if (s[i - 1] == '/') { slash = s + i - 1; break; }
    if (!slash) return cjf_strdup(".");
    size_t plen = (size_t)(slash - s);
    while (plen > 1 && s[plen - 1] == '/') plen--;
    if (plen == 0) return cjf_strdup("/");
    char *o = malloc(plen + 1);
    if (!o) return NULL;
    memcpy(o, s, plen);
    o[plen] = '\0';
    return o;
}

/* Python str.split() (no arg): runs of ASCII whitespace, no empties. */
static json_t *cjf_split_ws(const char *s) {
    json_t *arr = json_array();
    const char *p = s ? s : "";
    while (*p) {
        while (*p && cjf_is_ws((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !cjf_is_ws((unsigned char)*p)) p++;
        size_t len = (size_t)(p - start);
        char *tok = malloc(len + 1);
        if (tok) { memcpy(tok, start, len); tok[len] = '\0'; json_append(arr, json_string(tok)); free(tok); }
    }
    return arr;
}

/* Python int(str): optional ws/sign, digits with single underscores between
 * digits. Returns false on ValueError. */
static bool cjf_parse_int(const char *s, long long *out) {
    char *t = cjf_strip(s);
    if (!t) return false;
    const char *p = t;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!isdigit((unsigned char)*p)) { free(t); return false; }
    long long v = 0;
    bool prev_digit = false;
    for (; *p; p++) {
        if (isdigit((unsigned char)*p)) {
            v = v * 10 + (*p - '0');
            prev_digit = true;
        } else if (*p == '_' && prev_digit && isdigit((unsigned char)p[1])) {
            prev_digit = false;
        } else {
            free(t);
            return false;
        }
    }
    free(t);
    if (!prev_digit) return false;
    *out = neg ? -v : v;
    return true;
}

/* ── store-path seam helpers ───────────────────────────────────────────── */
/* _current_cron_store() seam; returns a malloc'd field string or NULL and
 * sets *exc to the owned sentinel on raise / missing field. */
static char *cjf_store_field(cjf_io_t *io, const char *field, json_t **exc) {
    *exc = NULL;
    json_t *st = cjf_seam(io, "_current_cron_store", cjf_args());
    if (cjf_raised(st)) { *exc = st; return NULL; }
    json_t *v = json_obj_get(st, field);
    if (!v || v->type != JSON_STRING) {
        json_free(st);
        char msg[128];
        snprintf(msg, sizeof msg, "'_CronStorePaths' object has no attribute '%s'", field);
        *exc = cjf_raise("AttributeError", msg);
        return NULL;
    }
    char *o = cjf_strdup(v->str_val);
    json_free(st);
    return o;
}

/* ── ported leaves / flows ─────────────────────────────────────────────── */

/* PoP: cjf_self_removal_delivery_allowed @ cron/jobs.py:self_removal_delivery_allowed */
json_t *cjf_self_removal_delivery_allowed(cjf_io_t *io, const char *job_id) {
    json_t *marker = json_obj_get(io->state, "self_removal_marker");
    const char *jid = job_id ? job_id : "";
    if (!marker || marker->type == JSON_NULL) return json_bool(false);
    const char *mjid = json_get_str(marker, "job_id", "");
    bool removed = json_get_bool(marker, "removed", false);
    if (strcmp(mjid, jid) != 0 || !removed) return json_bool(false);
    json_t *jobs = cjf_seam(io, "load_jobs", cjf_args());
    if (cjf_raised(jobs)) return jobs;
    if (jobs->type != JSON_ARRAY) {
        json_free(jobs);
        return cjf_raise("TypeError", "load_jobs() did not return a list");
    }
    bool allowed = true;
    for (size_t i = 0; i < jobs->c.count; i++) {
        json_t *item = jobs->c.items[i];
        if (!item || item->type != JSON_OBJECT) {
            char msg[96];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", cjf_py_typename(item));
            json_free(jobs);
            return cjf_raise("AttributeError", msg);
        }
        json_t *idv = json_obj_get(item, "id");
        /* item.get("id") != job_id: a non-string id never equals the str. */
        if (idv && idv->type == JSON_STRING && strcmp(idv->str_val ? idv->str_val : "", jid) == 0) {
            allowed = false;
            break;
        }
    }
    json_free(jobs);
    return json_bool(allowed);
}

/* poll_until(predicate, timeout, interval) for the fcntl _try_flock predicate
 * (agent/deadline.py helper; internal, no PoP — not a jobs.py def). The
 * predicate catches OSError only; other raises propagate. */
static json_t *cjf_poll_flock(cjf_io_t *io, double fd, double timeout) {
    json_t *a = cjf_args();
    json_t *m0 = cjf_seam(io, "time.monotonic", a);
    if (cjf_raised(m0)) return m0;
    double deadline = m0->num_val + timeout;
    json_free(m0);
    for (;;) {
        json_t *fa = cjf_args();
        json_set(fa, "fd", json_number(fd));
        json_set(fa, "op", json_string("LOCK_EX_NB"));
        json_t *r = cjf_seam(io, "fcntl.flock", fa);
        bool result;
        if (cjf_raised(r)) {
            if (cjf_oserror_kind(cjf_exc_kind(r))) { json_free(r); result = false; }
            else return r;  /* non-OSError escapes _try_flock */
        } else {
            json_free(r);
            result = true;
        }
        if (result) return json_bool(true);
        json_t *m1 = cjf_seam(io, "time.monotonic", cjf_args());
        if (cjf_raised(m1)) return m1;
        double remaining = deadline - m1->num_val;
        json_free(m1);
        if (remaining <= 0) return json_bool(false);
        double sl = CJF_FLOCK_POLL_INTERVAL < remaining ? CJF_FLOCK_POLL_INTERVAL : remaining;
        json_t *sa = cjf_args();
        json_set(sa, "seconds", json_number(sl));
        json_t *sr = cjf_seam(io, "time.sleep", sa);
        if (cjf_raised(sr)) return sr;
        json_free(sr);
    }
}

/* PoP: cjf_acquire_flock @ cron/jobs.py:_acquire_flock */
json_t *cjf_acquire_flock(cjf_io_t *io, double lock_fd, double timeout) {
    const char *plat = json_get_str(io->state, "platform", "fcntl");
    if (strcmp(plat, "fcntl") == 0)
        return cjf_poll_flock(io, lock_fd, timeout);
    if (strcmp(plat, "msvcrt") == 0) {
        json_t *a = cjf_args();
        json_set(a, "fd", json_number(lock_fd));  /* lock_fd.fileno() identity */
        json_set(a, "mode", json_string("LK_LOCK"));
        json_set(a, "nbytes", json_number(1));
        json_t *r = cjf_seam(io, "msvcrt.locking", a);
        if (cjf_raised(r)) return r;  /* no try/except on this branch */
        json_free(r);
        return json_bool(true);
    }
    return json_null();  /* no backend -> None */
}

/* PoP: cjf_release_flock @ cron/jobs.py:_release_flock */
json_t *cjf_release_flock(cjf_io_t *io, double lock_fd) {
    const char *plat = json_get_str(io->state, "platform", "fcntl");
    json_t *exc = NULL;
    if (strcmp(plat, "fcntl") == 0) {
        json_t *a = cjf_args();
        json_set(a, "fd", json_number(lock_fd));
        json_set(a, "op", json_string("LOCK_UN"));
        json_t *r = cjf_seam(io, "fcntl.flock", a);
        if (cjf_raised(r)) {
            if (cjf_oserror_kind(cjf_exc_kind(r))) json_free(r);
            else exc = r;
        } else json_free(r);
    } else if (strcmp(plat, "msvcrt") == 0) {
        json_t *a = cjf_args();
        json_set(a, "fd", json_number(lock_fd));
        json_set(a, "mode", json_string("LK_UNLCK"));
        json_set(a, "nbytes", json_number(1));
        json_t *r = cjf_seam(io, "msvcrt.locking", a);
        if (cjf_raised(r)) {
            if (cjf_oserror_kind(cjf_exc_kind(r))) json_free(r);
            else exc = r;
        } else json_free(r);
    }
    /* finally: lock_fd.close() — a raise here REPLACES any in-flight exc. */
    json_t *ca = cjf_args();
    json_set(ca, "fd", json_number(lock_fd));
    json_t *cr = cjf_seam(io, "close", ca);
    if (cjf_raised(cr)) {
        if (exc) json_free(exc);
        exc = cr;
    } else json_free(cr);
    return exc ? exc : json_null();
}

/* PoP: cjf_ensure_cron_dir @ cron/jobs.py:_ensure_cron_dir */
json_t *cjf_ensure_cron_dir(cjf_io_t *io, const char *cron_dir) {
    json_t *a = cjf_args();
    json_set(a, "path", json_string(cron_dir ? cron_dir : ""));
    json_t *named = cjf_seam(io, "_is_named_profile_path", a);
    if (cjf_raised(named)) return named;
    bool parents = !cjf_truthy(named);
    json_free(named);
    json_t *ma = cjf_args();
    json_set(ma, "path", json_string(cron_dir ? cron_dir : ""));
    json_set(ma, "parents", json_bool(parents));
    json_set(ma, "exist_ok", json_bool(true));
    json_t *r = cjf_seam(io, "mkdir", ma);
    if (cjf_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: cjf_cron_schedule @ cron/jobs.py:_cron_schedule */
json_t *cjf_cron_schedule(cjf_io_t *io, const char *expr, const char *display,
                          const char *missing_croniter, const char *invalid_label) {
    json_t *ok = cjf_seam(io, "_ensure_croniter", cjf_args());
    if (cjf_raised(ok)) return ok;
    bool have = cjf_truthy(ok);
    json_free(ok);
    if (!have) {
        const char *mc = missing_croniter ? missing_croniter : "";
        size_t n = strlen(mc) + 40;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n, "%s Install with: pip install croniter", mc);
        json_t *e = cjf_raise("ValueError", msg ? msg : "");
        free(msg);
        return e;
    }
    json_t *a = cjf_args();
    json_set(a, "expr", json_string(expr ? expr : ""));
    json_t *r = cjf_seam(io, "croniter", a);
    if (cjf_raised(r)) {
        char *es = cjf_exc_str(r);
        const char *il = invalid_label ? invalid_label : "";
        const char *dp = display ? display : "";
        size_t n = strlen(il) + strlen(dp) + strlen(es) + 24;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n, "Invalid %s '%s': %s", il, dp, es);
        json_t *e = cjf_raise("ValueError", msg ? msg : "");
        free(msg); free(es); json_free(r);
        return e;
    }
    json_free(r);
    json_t *out = json_object();
    json_set(out, "kind", json_string("cron"));
    json_set(out, "expr", json_string(expr ? expr : ""));
    json_set(out, "display", json_string(display ? display : ""));
    return out;
}

/* PoP: cjf_parse_aware @ cron/jobs.py:_parse_aware */
json_t *cjf_parse_aware(cjf_io_t *io, json_t *value, json_t *zone) {
    json_t *a = cjf_args();
    json_set(a, "value", value ? json_copy(value) : json_null());
    json_t *dt = cjf_seam(io, "datetime.fromisoformat", a);
    if (cjf_raised(dt)) {
        if (cjf_base_exc(dt)) return dt;
        json_free(dt);
        return json_null();
    }
    json_t *ea = cjf_args();
    json_set(ea, "dt", json_copy(dt));
    json_set(ea, "zone", zone ? json_copy(zone) : json_null());
    json_free(dt);
    json_t *r = cjf_seam(io, "_ensure_aware", ea);
    if (cjf_raised(r)) {
        if (cjf_base_exc(r)) return r;
        json_free(r);
        return json_null();
    }
    return r;
}

/* PoP: cjf_unlink_quiet @ cron/jobs.py:_unlink_quiet */
json_t *cjf_unlink_quiet(cjf_io_t *io, json_t *path) {
    if (!path || path->type == JSON_NULL) return json_null();
    json_t *a = cjf_args();
    json_set(a, "path", json_copy(path));
    json_t *r = cjf_seam(io, "os.unlink", a);
    if (cjf_raised(r)) {
        if (cjf_oserror_kind(cjf_exc_kind(r))) { json_free(r); return json_null(); }
        return r;  /* non-OSError (incl. BaseException) propagates */
    }
    json_free(r);
    return json_null();
}

/* PoP: cjf_append_telemetry_record @ cron/jobs.py:_append_telemetry_record */
json_t *cjf_append_telemetry_record(cjf_io_t *io, const char *filename,
                                    json_t *entry, json_t *recent) {
    /* recent.append(entry); del recent[:-_TELEMETRY_RECENT_HISTORY] */
    if (recent && recent->type == JSON_ARRAY) {
        json_append(recent, json_copy(entry));
        while (recent->c.count > CJF_TELEMETRY_RECENT_HISTORY) {
            json_free(recent->c.items[0]);
            json_array_remove(recent, 0);
        }
    }
    json_t *exc = NULL;
    char *cron_dir = cjf_store_field(io, "cron_dir", &exc);
    if (!cron_dir) goto except_check;
    char *path = cjf_path_join(cron_dir, filename);
    free(cron_dir);
    char *parent = cjf_dirname(path);
    /* _ensure_cron_dir is a ported REAL sibling (emits its own seams). */
    json_t *er = cjf_ensure_cron_dir(io, parent);
    free(parent);
    if (cjf_raised(er)) { exc = er; free(path); goto except_check; }
    json_free(er);
    json_t *oa = cjf_args();
    json_set(oa, "path", json_string(path));
    json_set(oa, "mode", json_string("a"));
    json_set(oa, "encoding", json_string("utf-8"));
    json_t *opened = cjf_seam(io, "open", oa);
    if (cjf_raised(opened)) { exc = opened; free(path); goto except_check; }
    json_free(opened);
    free(path);
    /* with open(...) as fh: fh.write(json.dumps(entry) + "\n") — the write
     * text is not trace-compared (name-only trace); compact serialize. */
    char *ser = json_serialize(entry);
    size_t sn = ser ? strlen(ser) : 0;
    char *text = malloc(sn + 2);
    if (text) { if (ser) memcpy(text, ser, sn); text[sn] = '\n'; text[sn + 1] = '\0'; }
    free(ser);
    json_t *wa = cjf_args();
    json_set(wa, "text", json_string(text ? text : "\n"));
    json_t *wr = cjf_seam(io, "write", wa);
    free(text);
    /* __exit__ runs even when write raised; a close raise replaces it. */
    json_t *cr = cjf_seam(io, "close", cjf_args());
    if (cjf_raised(wr)) {
        exc = wr;
        if (cjf_raised(cr)) { json_free(exc); exc = cr; }
        else json_free(cr);
    } else {
        json_free(wr);
        if (cjf_raised(cr)) exc = cr;
        else json_free(cr);
    }
except_check:
    if (exc) {
        if (cjf_base_exc(exc)) return exc;
        json_free(exc);
        cjf_log(io, "debug");
    }
    return json_null();
}

/* PoP: cjf_record_timezone_migration_catchup @ cron/jobs.py:_record_timezone_migration_catchup */
json_t *cjf_record_timezone_migration_catchup(cjf_io_t *io, json_t *job,
                                              const char *raw_iso, const char *norm_iso) {
    /* Module globals exist from import time; mirror them into state up front
     * so an early raise (the clock seam) still echoes them, like the oracle's
     * setdefault + unconditional sync. */
    double cnt = json_get_num(io->state, "_timezone_migration_catchups", 0);
    json_set(io->state, "_timezone_migration_catchups", json_number(cnt));
    json_t *recent = json_obj_get(io->state, "_timezone_migration_catchups_recent");
    if (!recent || recent->type != JSON_ARRAY) {
        recent = json_array();
        json_set(io->state, "_timezone_migration_catchups_recent", recent);
    }
    json_t *entry = json_object();
    json_t *jid = json_obj_get(job, "id");
    json_set(entry, "job_id", jid ? json_copy(jid) : json_null());
    json_t *name = json_obj_get(job, "name");
    json_t *namev = cjf_truthy(name) ? name : jid;
    json_set(entry, "name", namev ? json_copy(namev) : json_null());
    json_t *sched = json_obj_get(job, "schedule");
    json_t *expr = (sched && sched->type == JSON_OBJECT) ? json_obj_get(sched, "expr") : NULL;
    json_set(entry, "expr", expr ? json_copy(expr) : json_null());
    json_set(entry, "stored_next_run_at", json_string(raw_iso ? raw_iso : ""));
    json_set(entry, "normalized_next_run_at", json_string(norm_iso ? norm_iso : ""));
    json_t *now = cjf_seam(io, "_hermes_now", cjf_args());
    if (cjf_raised(now)) { json_free(entry); return now; }
    json_set(entry, "fired_at", json_copy(now));
    json_free(now);
    /* global _timezone_migration_catchups += 1 */
    json_set(io->state, "_timezone_migration_catchups", json_number(cnt + 1));
    json_t *r = cjf_append_telemetry_record(io, "timezone_migration_catchups.jsonl", entry, recent);
    json_free(entry);
    return r;
}

/* PoP: cjf_write_marker @ cron/jobs.py:_write_marker */
json_t *cjf_write_marker(cjf_io_t *io, const char *name, const char *text,
                         const char *tmp_prefix) {
    json_t *exc = NULL;
    json_t *ed = cjf_seam(io, "ensure_dirs", cjf_args());
    if (cjf_raised(ed)) { exc = ed; goto done; }
    json_free(ed);
    char *cron_dir = cjf_store_field(io, "cron_dir", &exc);
    if (!cron_dir) goto done;
    char *path = cjf_path_join(cron_dir, name);
    free(cron_dir);
    json_t *a = cjf_args();
    json_set(a, "path", json_string(path));
    json_set(a, "text", json_string(text ? text : ""));
    json_set(a, "tmp_prefix", json_string(tmp_prefix ? tmp_prefix : ""));
    json_set(a, "mode", json_number(384));  /* 0o600 */
    free(path);
    json_t *r = cjf_seam(io, "atomic_write_text", a);
    if (cjf_raised(r)) { exc = r; goto done; }
    json_free(r);
done:
    if (exc) {
        if (cjf_base_exc(exc)) return exc;
        json_free(exc);  /* except Exception: pass */
    }
    return json_null();
}

/* PoP: cjf_read_marker_fields @ cron/jobs.py:_read_marker_fields */
json_t *cjf_read_marker_fields(cjf_io_t *io, const char *name) {
    json_t *exc = NULL;
    char *cron_dir = cjf_store_field(io, "cron_dir", &exc);
    if (!cron_dir) goto fail;
    char *path = cjf_path_join(cron_dir, name);
    free(cron_dir);
    json_t *a = cjf_args();
    json_set(a, "path", json_string(path));
    json_set(a, "encoding", json_string("utf-8-sig"));
    free(path);
    json_t *r = cjf_seam(io, "read_text", a);
    if (cjf_raised(r)) { exc = r; goto fail; }
    const char *raw = (r->type == JSON_STRING && r->str_val) ? r->str_val : "";
    /* utf-8-sig strips a leading BOM. */
    if ((unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF)
        raw += 3;
    json_t *out = cjf_split_ws(raw);
    json_free(r);
    return out;
fail:
    if (exc) {
        if (cjf_base_exc(exc)) return exc;
        json_free(exc);
    }
    return json_array();
}

/* PoP: cjf_ticker_heartbeat_writer_alive @ cron/jobs.py:ticker_heartbeat_writer_alive */
json_t *cjf_ticker_heartbeat_writer_alive(cjf_io_t *io) {
    json_t *fields = cjf_read_marker_fields(io, "ticker_heartbeat");
    if (cjf_raised(fields)) return fields;
    /* try: import pid_exists_stdlib (stubbed present); len>=2 short-circuits
     * BEFORE int()/the seam call. */
    if (fields->type != JSON_ARRAY || fields->c.count < 2) {
        json_free(fields);
        return json_bool(false);
    }
    json_t *f1 = fields->c.items[1];
    const char *s = (f1 && f1->type == JSON_STRING) ? f1->str_val : NULL;
    long long pid = 0;
    if (!s || !cjf_parse_int(s, &pid)) {
        json_free(fields);  /* int() ValueError -> except -> False */
        return json_bool(false);
    }
    json_free(fields);
    json_t *a = cjf_args();
    json_set(a, "pid", json_number((double)pid));
    json_t *r = cjf_seam(io, "pid_exists_stdlib", a);
    if (cjf_raised(r)) {
        if (cjf_base_exc(r)) return r;
        json_free(r);
        return json_bool(false);
    }
    bool alive = cjf_truthy(r);
    json_free(r);
    return json_bool(alive);
}

/* PoP: cjf_unmerged_disk_jobs @ cron/jobs.py:_unmerged_disk_jobs */
json_t *cjf_unmerged_disk_jobs(cjf_io_t *io, json_t *jobs, json_t *removed_ids) {
    json_t *stamp = json_obj_get(io->state, "load_stamp");
    if (stamp && stamp->type != JSON_NULL) {
        json_t *exc = NULL;
        char *jf = cjf_store_field(io, "jobs_file", &exc);
        if (!jf) return exc;
        json_t *a = cjf_args();
        json_set(a, "path", json_string(jf));
        free(jf);
        json_t *st = cjf_seam(io, "_jobs_file_stamp", a);
        if (cjf_raised(st)) return st;
        bool match = cjf_json_eq(st, stamp);
        json_free(st);
        if (match) return json_array();
    }
    json_t *disk = cjf_seam(io, "_peek_jobs_unlocked", cjf_args());
    if (cjf_raised(disk)) return disk;
    if (!disk || disk->type == JSON_NULL) {
        if (disk) json_free(disk);
        json_t *exc = NULL;
        char *jf = cjf_store_field(io, "jobs_file", &exc);
        if (!jf) return exc;
        size_t n = strlen(jf) + 64;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n, "Cron database corrupted; refusing to overwrite %s", jf);
        json_t *e = cjf_raise("RuntimeError", msg ? msg : "");
        free(msg); free(jf);
        return e;
    }
    /* seen = {str(j["id"]) for j in jobs if dict and truthy id} | removed */
    json_t *seen = json_object();  /* set model: keys only */
    for (size_t i = 0; jobs && i < json_len(jobs); i++) {
        json_t *j = json_get(jobs, i);
        if (!j || j->type != JSON_OBJECT) continue;
        json_t *idv = json_obj_get(j, "id");
        if (!cjf_truthy(idv)) continue;
        char *sid = cjf_py_str(idv);
        json_set(seen, sid, json_bool(true));
        free(sid);
    }
    for (size_t i = 0; removed_ids && i < json_len(removed_ids); i++) {
        json_t *rid = json_get(removed_ids, i);
        if (!cjf_truthy(rid)) continue;
        char *sid = cjf_py_str(rid);
        json_set(seen, sid, json_bool(true));
        free(sid);
    }
    json_t *recovered = json_array();
    for (size_t i = 0; i < json_len(disk); i++) {
        json_t *dj = json_get(disk, i);
        if (!dj || dj->type != JSON_OBJECT) continue;
        json_t *idv = json_obj_get(dj, "id");
        if (!cjf_truthy(idv)) continue;
        char *sid = cjf_py_str(idv);
        if (!json_obj_get(seen, sid)) {
            json_append(recovered, json_copy(dj));
            json_set(seen, sid, json_bool(true));
        }
        free(sid);
    }
    json_free(seen);
    json_free(disk);
    return recovered;
}
/* PoP: cjf_stage_jobs_payload @ cron/jobs.py:_stage_jobs_payload */
json_t *cjf_stage_jobs_payload(cjf_io_t *io, const char *jobs_file, json_t *jobs) {
    json_t *a = cjf_args();
    json_set(a, "path", json_string(jobs_file ? jobs_file : ""));
    json_set(a, "suffix", json_string(".tmp"));
    json_set(a, "prefix", json_string(".jobs_"));
    json_t *mk = cjf_seam(io, "mkstemp_beside", a);
    if (cjf_raised(mk)) return mk;
    if (mk->type != JSON_ARRAY || mk->c.count < 2) {
        json_free(mk);
        return cjf_raise("TypeError", "cannot unpack non-iterable mkstemp_beside result");
    }
    json_t *fdn = json_get(mk, 0);
    double fd = (fdn && fdn->type == JSON_NUMBER) ? fdn->num_val : 0;
    json_t *tmpn = json_get(mk, 1);
    char *tmp_path = cjf_strdup(tmpn && tmpn->type == JSON_STRING ? tmpn->str_val : "");
    json_free(mk);

    json_t *exc = NULL;
    bool in_with = false;
    /* os.fdopen(fd, "w", encoding="utf-8") — enters the with on success. */
    json_t *fa = cjf_args();
    json_set(fa, "fd", json_number(fd));
    json_set(fa, "mode", json_string("w"));
    json_set(fa, "encoding", json_string("utf-8"));
    json_t *fo = cjf_seam(io, "fdopen", fa);
    if (cjf_raised(fo)) { exc = fo; goto cleanup; }
    json_free(fo);
    in_with = true;
    /* payload = {"jobs": jobs, "updated_at": _hermes_now().isoformat()} */
    json_t *now = cjf_seam(io, "_hermes_now", cjf_args());
    if (cjf_raised(now)) { exc = now; goto cleanup; }
    json_t *payload = json_object();
    json_set(payload, "jobs", jobs ? json_copy(jobs) : json_array());
    json_set(payload, "updated_at", json_copy(now));
    json_free(now);
    json_t *da = cjf_args();
    json_set(da, "payload", payload);  /* json_set ADOPTS payload */
    json_set(da, "indent", json_number(2));
    json_set(da, "ensure_ascii", json_bool(false));
    json_t *dr = cjf_seam(io, "json_dump", da);
    if (cjf_raised(dr)) { exc = dr; goto cleanup; }
    json_free(dr);
    json_t *fl = cjf_seam(io, "flush", cjf_args());
    if (cjf_raised(fl)) { exc = fl; goto cleanup; }
    json_free(fl);
    json_t *fn = cjf_seam(io, "fileno", cjf_args());
    if (cjf_raised(fn)) { exc = fn; goto cleanup; }
    double fno = (fn->type == JSON_NUMBER) ? fn->num_val : fd;
    json_free(fn);
    json_t *sa = cjf_args();
    json_set(sa, "fd", json_number(fno));
    json_t *sr = cjf_seam(io, "fsync", sa);
    if (cjf_raised(sr)) { exc = sr; goto cleanup; }
    json_free(sr);
cleanup:
    /* with __exit__ (close) runs inside the try when fdopen succeeded; a
     * close raise replaces the in-flight exception. */
    if (in_with) {
        json_t *cr = cjf_seam(io, "close", cjf_args());
        if (cjf_raised(cr)) {
            if (exc) json_free(exc);
            exc = cr;
        } else json_free(cr);
    }
    /* except BaseException: _unlink_quiet(tmp_path); raise */
    if (exc) {
        json_t *tp = json_string(tmp_path);
        json_t *ur = cjf_unlink_quiet(io, tp);
        json_free(tp);
        if (cjf_raised(ur)) { json_free(exc); exc = ur; }
        else json_free(ur);
        free(tmp_path);
        return exc;
    }
    json_t *ret = json_string(tmp_path);
    free(tmp_path);
    return ret;
}

/* PoP: cjf_with_job @ cron/jobs.py:_with_job */
json_t *cjf_with_job(cjf_io_t *io, json_t *job_id, json_t *missing) {
    json_t *enter = cjf_seam(io, "jobs_lock.enter", cjf_args());
    if (cjf_raised(enter)) return enter;  /* __enter__ failed: no __exit__ */
    json_free(enter);
    json_t *exc = NULL;
    json_t *ret = NULL;
    json_t *jobs = cjf_seam(io, "load_jobs", cjf_args());
    if (cjf_raised(jobs)) { exc = jobs; jobs = NULL; goto exit; }
    if (jobs->type != JSON_ARRAY) {
        char msg[96];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", cjf_py_typename(jobs));
        json_free(jobs);
        jobs = NULL;
        exc = cjf_raise("TypeError", msg);
        goto exit;
    }
    for (size_t i = 0; i < jobs->c.count; i++) {
        json_t *job = jobs->c.items[i];
        if (!job || job->type != JSON_OBJECT) {
            char msg[96];
            snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", cjf_py_typename(job));
            exc = cjf_raise("AttributeError", msg);
            goto exit;
        }
        json_t *idv = json_obj_get(job, "id");
        if (cjf_json_eq(idv, job_id)) {
            json_t *fa = cjf_args();
            json_set(fa, "jobs", json_copy(jobs));
            json_set(fa, "i", json_number((double)i));
            json_set(fa, "job", json_copy(job));
            json_t *fr = cjf_seam(io, "fn", fa);
            if (cjf_raised(fr)) { exc = fr; goto exit; }
            ret = fr;
            goto exit;
        }
    }
    ret = missing ? json_copy(missing) : json_null();
exit:
    if (jobs) json_free(jobs);
    json_t *ex = cjf_seam(io, "jobs_lock.exit", cjf_args());
    if (cjf_raised(ex)) {
        /* __exit__ raise replaces the in-flight exception / return. */
        if (exc) json_free(exc);
        if (ret) json_free(ret);
        return ex;
    }
    json_free(ex);
    if (exc) { if (ret) json_free(ret); return exc; }
    return ret ? ret : json_null();
}

/* PoP: cjf_next_run_or_reject_past_oneshot @ cron/jobs.py:_next_run_or_reject_past_oneshot */
json_t *cjf_next_run_or_reject_past_oneshot(cjf_io_t *io, json_t *parsed_schedule,
                                            const char *label, json_t *fallback_run_at,
                                            const char *what, json_t *timezone) {
    (void)label; (void)what;  /* only feed the logger.warning args (name-only trace) */
    json_t *a = cjf_args();
    json_set(a, "schedule", parsed_schedule ? json_copy(parsed_schedule) : json_null());
    json_set(a, "timezone", timezone ? json_copy(timezone) : json_null());
    json_t *next = cjf_seam(io, "compute_next_run", a);
    if (cjf_raised(next)) return next;
    const char *kind = json_get_str(parsed_schedule, "kind", "");
    bool is_none = !next || next->type == JSON_NULL;
    if (strcmp(kind, "once") == 0 && is_none) {
        json_t *run_at = json_obj_get(parsed_schedule, "run_at");
        if (!cjf_truthy(run_at)) run_at = fallback_run_at;
        cjf_log(io, "warning");
        char *rs = cjf_py_str(run_at);
        size_t n = strlen(rs) + 128;
        char *msg = malloc(n);
        if (msg) snprintf(msg, n,
            "Requested one-shot time %s is more than %ds in the past and cannot be scheduled.",
            rs, CJF_ONESHOT_GRACE_SECONDS);
        json_t *e = cjf_raise("ValueError", msg ? msg : "");
        free(msg); free(rs); json_free(next);
        return e;
    }
    return next;
}

/* PoP: cjf_apply_pin_update @ cron/jobs.py:_apply_pin_update */
json_t *cjf_apply_pin_update(cjf_io_t *io, json_t *job, json_t *updates) {
    if (!updates || updates->type != JSON_OBJECT || !json_has(updates, "pinned"))
        return json_null();
    json_t *pv = json_obj_get(updates, "pinned");
    bool pinned = cjf_truthy(pv);
    json_obj_del(updates, "pinned");  /* updates.pop("pinned") */
    if (json_has(updates, "model")) return json_null();
    if (pinned) {
        json_t *m = job ? json_obj_get(job, "model") : NULL;
        json_t *na = cjf_args();
        json_set(na, "value", m ? json_copy(m) : json_null());
        json_t *norm = cjf_seam(io, "_normalize_job_optional_text", na);
        if (cjf_raised(norm)) return norm;
        bool has_model = cjf_truthy(norm);
        json_free(norm);
        if (!has_model) {
            json_t *pin = cjf_seam(io, "_main_model_pin", cjf_args());
            if (cjf_raised(pin)) return pin;
            if (pin->type != JSON_ARRAY || pin->c.count < 2) {
                json_free(pin);
                return cjf_raise("TypeError", "cannot unpack non-iterable _main_model_pin result");
            }
            json_set(updates, "provider", json_copy(json_get(pin, 0)));
            json_set(updates, "model", json_copy(json_get(pin, 1)));
            json_free(pin);
        }
    } else {
        json_set(updates, "provider", json_null());
        json_set(updates, "model", json_null());
    }
    return json_null();
}

/* PoP: cjf_refresh_claim @ cron/jobs.py:_refresh_claim */
json_t *cjf_refresh_claim(cjf_io_t *io, json_t *jobs, json_t *claim,
                          const char *expected_owner) {
    if (!claim || claim->type != JSON_OBJECT) return json_bool(false);
    json_t *by = json_obj_get(claim, "by");
    const char *eo = expected_owner ? expected_owner : "";
    if (!by || by->type != JSON_STRING || strcmp(by->str_val ? by->str_val : "", eo) != 0)
        return json_bool(false);
    json_t *now = cjf_seam(io, "_hermes_now", cjf_args());
    if (cjf_raised(now)) return now;
    json_set(claim, "at", json_copy(now));
    json_free(now);
    json_t *sa = cjf_args();
    json_set(sa, "jobs", jobs ? json_copy(jobs) : json_array());
    json_t *sr = cjf_seam(io, "save_jobs", sa);
    if (cjf_raised(sr)) return sr;
    json_free(sr);
    return json_bool(true);
}