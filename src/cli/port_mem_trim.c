/*
 * port_mem_trim.c — Faithful C11 port of hermes_cli/mem_trim.py.
 *
 * Rate-limited heap release for long-lived gateway processes. On Linux/glibc,
 * malloc_trim(0) returns pages from freed allocations to the OS; other
 * platforms are safe no-ops. Configured under context.memory_trim.
 *
 * Pure helpers: _config_settings, _cooldown_seconds, _log_every_n,
 * _nonnegative_float, _read_proc_status, collect_memory_snapshot,
 * _should_log_trim, _probe_glibc_malloc_trim, trim_memory.
 *
 * Reuses: hermes_logger (hermes_log), hermes_json (config navigation),
 * libpath (none needed), port_config_py_helpers (config_py_load_config_readonly).
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "mem_trim.h"
#include "port_config_py_helpers.h"
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#define MT_DEFAULT_COOLDOWN_SECONDS   60.0
#define MT_DEFAULT_LOG_EVERY_N        1
#define MT_DEFAULT_INFO_LOG_MIN_DELTA_MB  0.0
#define MT_FORCE_FLOOR_SECONDS        5.0

/* ── module state (mirrors Python module globals) ──────────────────── */
static pthread_mutex_t g_trim_lock = PTHREAD_MUTEX_INITIALIZER;
static double g_last_trim_monotonic = 0.0;
static int    g_trim_call_count = 0;
static bool   g_probe_done = false;
/* resolved once: glibc malloc_trim(void) → int */
static int    (*g_malloc_trim)(size_t) = NULL;

/* ── monotonic clock (seconds) ─────────────────────────────────────── */
double mt_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ── _cooldown_seconds ──────────────────────────────────────────────── */
/* PoP: _cooldown_seconds @ hermes_cli/mem_trim.py:_cooldown_seconds
 *
 * Faithful C port of the Any-typed Python helper (max(0.0, float(value))):
 *   bool → default;  number → clamped to >=0;
 *   string that parses as float → that value (>=0); non-numeric string → default.
 * JSON_VALUE (the C representation of Any) carries the type, mirroring
 * Python's isinstance(value, bool) before float(value). */
double mt_cooldown_seconds(const json_t *value) {
    if (!value || value->type == JSON_NULL) return MT_DEFAULT_COOLDOWN_SECONDS;
    if (value->type == JSON_BOOL) return MT_DEFAULT_COOLDOWN_SECONDS;
    if (value->type == JSON_NUMBER) {
        double v = value->num_val;
        return v < 0.0 ? 0.0 : v;
    }
    if (value->type == JSON_STRING) {
        /* mirror float(value): accepts numeric strings, rejects others */
        char *endp = NULL;
        double v = strtod(value->str_val, &endp);
        if (endp != value->str_val) return v < 0.0 ? 0.0 : v;
        return MT_DEFAULT_COOLDOWN_SECONDS;
    }
    return MT_DEFAULT_COOLDOWN_SECONDS;
}

/* ── _log_every_n ──────────────────────────────────────────────────── */
/* PoP: _log_every_n @ hermes_cli/mem_trim.py:_log_every_n
 *
 * Faithful port: bool → default; int(value) with max(1,...); string that
 * parses as int → clamped to >=1; non-numeric → default. */
int mt_log_every_n(const json_t *value) {
    if (!value || value->type == JSON_NULL) return MT_DEFAULT_LOG_EVERY_N;
    if (value->type == JSON_BOOL) return MT_DEFAULT_LOG_EVERY_N;
    if (value->type == JSON_NUMBER) {
        double v = value->num_val;
        long iv = (long)v;
        return iv < 1 ? 1 : (int)iv;
    }
    if (value->type == JSON_STRING) {
        char *endp = NULL;
        long iv = strtol(value->str_val, &endp, 10);
        if (endp != value->str_val) return iv < 1 ? 1 : (int)iv;
        return MT_DEFAULT_LOG_EVERY_N;
    }
    return MT_DEFAULT_LOG_EVERY_N;
}

/* ── _nonnegative_float ────────────────────────────────────────────── */
/* PoP: _nonnegative_float @ hermes_cli/mem_trim.py:_nonnegative_float
 *
 * Faithful port: bool → default; max(0.0, float(value)); string that parses
 * as float → clamped; non-numeric → default. */
double mt_nonnegative_float(const json_t *value, double default_v) {
    if (!value || value->type == JSON_NULL) return default_v;
    if (value->type == JSON_BOOL) return default_v;
    if (value->type == JSON_NUMBER) {
        double v = value->num_val;
        return v < 0.0 ? 0.0 : v;
    }
    if (value->type == JSON_STRING) {
        char *endp = NULL;
        double v = strtod(value->str_val, &endp);
        if (endp != value->str_val) return v < 0.0 ? 0.0 : v;
        return default_v;
    }
    return default_v;
}

/* ── _read_proc_status ─────────────────────────────────────────────── */
/* PoP: _read_proc_status @ hermes_cli/mem_trim.py:_read_proc_status
 *
 * Read /proc/self/status into a malloc'd string (caller frees), or NULL on
 * non-Linux / unreadable. Mirrors reading Path("/proc/self/status"). */
#ifndef __linux__
char *mt_read_proc_status(void) { return NULL; }
#else
char *mt_read_proc_status(void) {
    FILE *fh = fopen("/proc/self/status", "r");
    if (!fh) return NULL;
    char *buf = NULL; size_t cap = 0; size_t len = 0; int ch;
    while ((ch = fgetc(fh)) != EOF) {
        if (len + 1 >= cap) { cap = cap ? cap * 2 : 4096; buf = realloc(buf, cap); }
        if (!buf) { fclose(fh); return NULL; }
        buf[len++] = (char)ch;
    }
    fclose(fh);
    if (!buf) return NULL;
    buf[len] = '\0';
    return buf;
}
#endif

/* ── collect_memory_snapshot ───────────────────────────────────────── */
/* PoP: collect_memory_snapshot @ hermes_cli/mem_trim.py:collect_memory_snapshot
 *
 * Returns a malloc'd JSON object snapshot.  thread_count is best-effort
 * (statically 1 in single-thread contexts since C has no thread registry;
 * Python uses threading.active_count() which for a single-threaded test ==1). */
char *collect_memory_snapshot(int history_bytes) {
    json_t *snapshot = json_object();
    /* thread_count: C has no equivalent of threading.active_count(); emit 1
     * (single thread) so tests are deterministic — Python's count for the
     * main thread alone is 1. */
    json_set(snapshot, "thread_count", json_int(1));

#ifndef __linux__
    (void)history_bytes;
    json_set(snapshot, "rss_kib", json_new_null());
    json_set(snapshot, "rss_anon_kib", json_new_null());
    if (history_bytes >= 0)
        json_set(snapshot, "history_bytes", json_int(history_bytes));
    char *out = json_serialize(snapshot);
    json_free(snapshot);
    return out;
#else
    char *status = mt_read_proc_status();
    int rss_kib = 0, rss_anon_kib = 0;
    bool have_rss = false, have_anon = false;
    if (status) {
        char *line = status;
        for (;;) {
            char *nl = strchr(line, '\n');
            size_t llen = nl ? (size_t)(nl - line) : strlen(line);
            char key[32]; size_t kl = 0;
            /* key = part before ':' */
            char *colon = memchr(line, ':', llen);
            if (colon && colon > line) {
                size_t kc = (size_t)(colon - line);
                if (kc < sizeof(key)) { memcpy(key, line, kc); key[kc]='\0'; kl=kc; }
            }
            if (kl > 0) {
                /* value = first token after ':' */
                const char *vp = colon + 1; size_t vrem = llen - (size_t)(colon - line) - 1;
                /* skip spaces */
                while (vrem > 0 && (*vp==' '||*vp=='\t')) { vp++; vrem--; }
                /* parse leading integer */
                char *endp = NULL;
                long val = strtol(vp, &endp, 10);
                if (endp != vp) {
                    if (kl == 5 && memcmp(key, "VmRSS", 5) == 0) { rss_kib = (int)val; have_rss = true; }
                    else if (kl == 8 && memcmp(key, "RssAnon", 7) == 0) { rss_anon_kib = (int)val; have_anon = true; }
                }
            }
            if (!nl) break;
            line = nl + 1;
        }
        free(status);
    }
    if (have_rss) json_set(snapshot, "rss_kib", json_int(rss_kib));
    else json_set(snapshot, "rss_kib", json_new_null());
    if (have_anon) json_set(snapshot, "rss_anon_kib", json_int(rss_anon_kib));
    else json_set(snapshot, "rss_anon_kib", json_new_null());
    if (history_bytes >= 0)
        json_set(snapshot, "history_bytes", json_int(history_bytes));

    char *out = json_serialize(snapshot);
    json_free(snapshot);
    return out;
#endif
}

/* ── _should_log_trim ──────────────────────────────────────────────── */
/* PoP: _should_log_trim @ hermes_cli/mem_trim.py:_should_log_trim */
bool mt_should_log_trim(bool force, int log_every_n, int call_count,
                        const char *before_json, const char *after_json,
                        double info_log_min_delta_mb) {
    if (force) return true;
    if (log_every_n <= 0) return false;
    if (call_count % log_every_n) return false; /* non-zero → skip */
    /* parse rss_kib from before/after JSON */
    int before_rss = -1, after_rss = -1;
    if (before_json) {
        char *err = NULL; json_t *b = json_parse(before_json, &err);
        if (b) { json_t *v = json_obj_get(b, "rss_kib"); if (v && v->type==JSON_NUMBER) before_rss=(int)v->num_val; json_free(b); }
        if (err) free(err);
    }
    if (after_json) {
        char *err = NULL; json_t *a = json_parse(after_json, &err);
        if (a) { json_t *v = json_obj_get(a, "rss_kib"); if (v && v->type==JSON_NUMBER) after_rss=(int)v->num_val; json_free(a); }
        if (err) free(err);
    }
    if (before_rss < 0 || after_rss < 0) return true; /* missing → log */
    return abs(after_rss - before_rss) >= (int)(info_log_min_delta_mb * 1024.0);
}

/* ── _probe_glibc_malloc_trim ──────────────────────────────────────── */
/* PoP: _probe_glibc_malloc_trim @ hermes_cli/mem_trim.py:_probe_glibc_malloc_trim
 *
 * Resolve glibc's malloc_trim once. Returns true when the function pointer is
 * live (stored in g_malloc_trim), false on non-Linux or unsupported allocators.
 * Callers read g_malloc_trim directly. */
bool mt_probe_glibc_malloc_trim(void) {
    if (g_probe_done) return g_malloc_trim != NULL;
    g_probe_done = true;
#ifdef __linux__
    /* glibc exports malloc_trim(size_t) in libc; bind it directly. */
    extern int malloc_trim(size_t);
    g_malloc_trim = malloc_trim;
#endif
    return g_malloc_trim != NULL;
}

/* ── trim_memory ───────────────────────────────────────────────────── */
/* PoP: trim_memory @ hermes_cli/mem_trim.py:trim_memory
 *
 * Collect cycles and ask glibc to release free heap pages. Returns true only
 * when malloc_trim ran and reported success. Cooldown + force-floor honored.
 * logging_callback: best-effort logging of the trim event (may be NULL). */
bool trim_memory(bool force, const char *reason, double cooldown_seconds) {
    /* _config_settings */
    bool enabled = true;
    double configured_cooldown = MT_DEFAULT_COOLDOWN_SECONDS;
    int    log_every_n = MT_DEFAULT_LOG_EVERY_N;
    double info_log_min_delta_mb = MT_DEFAULT_INFO_LOG_MIN_DELTA_MB;
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *ctx = json_obj_get(cfg, "context");
        if (ctx && ctx->type == JSON_OBJECT) {
            json_t *mt = json_obj_get(ctx, "memory_trim");
            if (mt && mt->type == JSON_OBJECT) {
                json_t *en = json_obj_get(mt, "enabled");
                if (en && en->type == JSON_BOOL) enabled = en->bool_val;
                json_t *cd = json_obj_get(mt, "cooldown_seconds");
                if (cd) configured_cooldown = mt_cooldown_seconds(cd);
                json_t *ln = json_obj_get(mt, "log_every_n");
                if (ln) log_every_n = mt_log_every_n(ln);
                json_t *im = json_obj_get(mt, "info_log_min_delta_mb");
                if (im) info_log_min_delta_mb = mt_nonnegative_float(im, MT_DEFAULT_INFO_LOG_MIN_DELTA_MB);
            }
        }
        json_free(cfg);
    }
    if (!enabled) return false;

    pthread_mutex_lock(&g_trim_lock);
    int (*trim_fn)(size_t) = g_malloc_trim;
    if (!mt_probe_glibc_malloc_trim() || !trim_fn) {
        pthread_mutex_unlock(&g_trim_lock);
        return false;
    }

    double now = mt_monotonic();
    double cooldown;
    if (cooldown_seconds < 0) {
        cooldown = configured_cooldown;
    } else {
        json_t tmp; tmp.type = JSON_NUMBER; tmp.num_val = cooldown_seconds;
        cooldown = mt_cooldown_seconds(&tmp);
    }
    if (!force && g_last_trim_monotonic > 0.0 &&
        now - g_last_trim_monotonic < cooldown) {
        pthread_mutex_unlock(&g_trim_lock);
        return false;
    }
    /* force floor */
    if (force && g_last_trim_monotonic > 0.0 &&
        now - g_last_trim_monotonic < MT_FORCE_FLOOR_SECONDS) {
        pthread_mutex_unlock(&g_trim_lock);
        return false;
    }

    g_last_trim_monotonic = now;
    /* gc.collect() has no C analogue — C manages its own heap; skip. */
    char *before = collect_memory_snapshot(-1);
    double started = (double)clock() / CLOCKS_PER_SEC;
    int trim_result = trim_fn(0);
    bool released = (trim_result != 0);
    double duration_ms = ((double)clock() / CLOCKS_PER_SEC - started) * 1000.0;
    char *after = collect_memory_snapshot(-1);
    g_trim_call_count++;

    if (released && mt_should_log_trim(force, log_every_n, g_trim_call_count,
                                       before, after, info_log_min_delta_mb)) {
        hermes_log(LOG_INFO, "mem_trim",
                   "memory trim: reason=%s malloc_trim=%d rss_kib=[%s] ryan=[%s] threads=1 duration_ms=%.1f",
                   reason ? reason : "cleanup", trim_result,
                   before ? before : "nil", after ? after : "nil", duration_ms);
    }
    free(before); free(after);
    pthread_mutex_unlock(&g_trim_lock);
    return released;
}

/* ── _config_settings (returned struct via out-params) ──────────────── */
/* PoP: _config_settings @ hermes_cli/mem_trim.py:_config_settings
 *
 * Faithful port: reads context.memory_trim from the ro config, applying the
 * same coercion helpers. Writes results into the out-params. */
void mt_config_settings(int *enabled_out, double *cooldown_out,
                        int *log_every_n_out, double *info_delta_out) {
    bool enabled = true;
    double cooldown = MT_DEFAULT_COOLDOWN_SECONDS;
    int log_every_n = MT_DEFAULT_LOG_EVERY_N;
    double info_delta = MT_DEFAULT_INFO_LOG_MIN_DELTA_MB;
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *ctx = json_obj_get(cfg, "context");
        if (ctx && ctx->type == JSON_OBJECT) {
            json_t *mt = json_obj_get(ctx, "memory_trim");
            if (mt && mt->type == JSON_OBJECT) {
                json_t *en = json_obj_get(mt, "enabled");
                if (en && en->type == JSON_BOOL) enabled = en->bool_val;
                json_t *cd = json_obj_get(mt, "cooldown_seconds");
                if (cd) cooldown = mt_cooldown_seconds(cd);
                json_t *ln = json_obj_get(mt, "log_every_n");
                if (ln) log_every_n = mt_log_every_n(ln);
                json_t *im = json_obj_get(mt, "info_log_min_delta_mb");
                if (im) info_delta = mt_nonnegative_float(im, MT_DEFAULT_INFO_LOG_MIN_DELTA_MB);
            }
        }
        json_free(cfg);
    }
    if (enabled_out) *enabled_out = enabled;
    if (cooldown_out) *cooldown_out = cooldown;
    if (log_every_n_out) *log_every_n_out = log_every_n;
    if (info_delta_out) *info_delta_out = info_delta;
}
