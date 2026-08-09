/*
 * port_tools_registry_cache.c — Module-level discovery + check_fn cache layer.
 *
 * Faithful C11 port of the module-level cache helpers in tools/registry.py:
 *   _discovery_cache_path, _load_discovery_cache, _save_discovery_cache,
 *   _check_fn_cache / _check_fn_last_good (the in-memory verdict dict),
 *   _prune_check_fn_caches, check_fn_cache_scope,
 *   get_cached_check_fn_result.
 *
 * The C registry already keeps a per-tool 30s ``check_fn_last`` timestamp
 * (see registry.c registry_refresh_availability); this module adds the
 * *read-only* verdict cache and the durable discovery-cache that the Python
 * module exposes so non-calling surfaces (dashboard status panels) can read
 * the last-known verdict without re-running probes, and so tool-discovery
 * verdicts survive restarts.
 *
 * Concurrency: the Python module guards the dicts with ``_check_fn_cache_lock``.
 * The C registry is single-threaded for availability checks (refresh happens
 * on the agent thread, not in request handlers); a pthread mutex is kept for
 * parity + future gateway hardening.
 */

#include "hermes_json.h"
#include "slermes_home.h"       /* slermes_home() — the HERMES_HOME path */
#include "hermes_logger.h"      /* hermes_log / LOG_DEBUG */
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

#define CHECK_FN_TTL_SECONDS         30.0
#define CHECK_FN_FAILURE_GRACE_SECONDS 60.0
#define CHECK_FN_CACHE_MAX           512

/* Sentinel: multiplex profile identity unresolved. check_fn_cache_scope
 * returns the empty string ("") in that fail-closed case.  NULL = process-wide
 * cache active (single-home). */
#define CHECK_FN_CACHE_BYPASS ""

/* ── in-memory verdict cache ────────────────────────────────────────── */
/*
 * Each entry maps (check_fn pointer, scope) -> {timestamp, value}.
 * We key by the function-pointer address (stable per process) plus the scope
 * string, mirroring Python's (fn, scope) tuple key.
 */
typedef struct {
    void   *fn;       /* check_fn address */
    char   *scope;    /* profile key or NULL (process-wide) */
    double  ts;       /* monotonic seconds at write */
    bool    value;    /* cached verdict */
    /* last-good tracking: separate from the verdict cache so a transient
     * False within the grace window does not evict a recent True. */
    double  last_good_ts;
    bool    last_good_valid;
} cfn_entry_t;

#define CFN_CACHE_INIT 128
static cfn_entry_t *g_cfn_cache = NULL;
static size_t       g_cfn_count = 0;
static size_t       g_cfn_cap   = 0;
static pthread_mutex_t g_cfn_lock = PTHREAD_MUTEX_INITIALIZER;

/* ── monotonic clock ────────────────────────────────────────────────── */
static double cfn_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

/* ── _discovery_cache_path ──────────────────────────────────────────── */
/* PoP: _discovery_cache_path @ tools/registry.py:_discovery_cache_path */
/* Returns "<hermes_home>/cache/tool_discovery_cache.json" (malloc'd), or NULL
 * if the hermes home cannot be resolved (mirrors the except→None path). */
char *discovery_cache_path(void) {
    const char *home = slermes_home();
    if (!home || !home[0]) return NULL;
    /* cache subdir may not exist yet — we return the path regardless */
    size_t need = strlen(home) + strlen("/cache/tool_discovery_cache.json") + 1;
    char *p = malloc(need);
    if (!p) return NULL;
    snprintf(p, need, "%s/cache/tool_discovery_cache.json", home);
    return p;
}

/* ── _load_discovery_cache ──────────────────────────────────────────── */
/* PoP: _load_discovery_cache @ tools/registry.py:_load_discovery_cache */
json_t *load_discovery_cache(void) {
    char *path = discovery_cache_path();
    json_t *out = json_object();
    if (!path) return out;          /* home unresolvable → empty dict */

    char *data = NULL;
    FILE *fh = fopen(path, "r");
    if (fh) {
        fseek(fh, 0, SEEK_END); long sz = ftell(fh); fseek(fh, 0, SEEK_SET);
        if (sz > 0) {
            data = malloc((size_t)sz + 1);
            if (data) {
                size_t rd = fread(data, 1, (size_t)sz, fh);
                data[rd] = '\0';
            }
        }
        fclose(fh);
    }
    free(path);

    if (data) {
        char *err = NULL;
        json_t *parsed = json_parse(data, &err);
        free(data);
        if (parsed && parsed->type == JSON_OBJECT) {
            json_free(out);
            return parsed;          /* real cache loaded */
        }
        if (parsed) json_free(parsed);
        if (err) free(err);
    }
    /* any error → empty dict (full scan) */
    return out;
}

/* ── _save_discovery_cache (best-effort atomic write) ───────────────── */
/* PoP: _save_discovery_cache @ tools/registry.py:_save_discovery_cache */
void save_discovery_cache(const json_t *cache) {
    char *path = discovery_cache_path();
    if (!path) return;
    char *serialized = json_serialize(cache);
    if (!serialized) { free(path); return; }

    /* temp path: "<path>.tmp.<pid>" */
    size_t plen = strlen(path);
    size_t tlen = plen + 32;
    char *tmp = malloc(tlen);
    if (!tmp) { free(path); free(serialized); return; }
    snprintf(tmp, tlen, "%s.tmp.%d", path, (int)getpid());

    FILE *fh = fopen(tmp, "w");
    if (!fh) { free(tmp); free(path); free(serialized); return; }
    fputs(serialized, fh);
    fflush(fh);
    /* best-effort fsync for durability parity */
    fsync(fileno(fh));
    fclose(fh);
    /* atomic rename: tmp → path */
    rename(tmp, path);
    free(tmp); free(path); free(serialized);
}

/* ── helpers ───────────────────────────────────────────────────────── */
/* Two scopes match: both NULL (process-wide) or both non-NULL & equal. */
static bool scope_match(const char *a, const char *b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

/* ── check_fn_cache_scope ───────────────────────────────────────────── */
/* PoP: check_fn_cache_scope @ tools/registry.py:check_fn_cache_scope */
char *check_fn_cache_scope(void) {
    /* is_multiplex_active is not wired in the C single-home agent; single-
     * profile processes keep the historical process-wide cache (scope==NULL,
     * cache ACTIVE — NOT a bypass).  Fail-closed bypass is the empty string
     * (CHECK_FN_CACHE_BYPASS) and only arises when multiplex is active but
     * the profile override can't be resolved — not reachable in single-home. */
    return NULL;
}

/* ── _prune_check_fn_caches ────────────────────────────────────────── */
/* PoP: _prune_check_fn_caches @ tools/registry.py:_prune_check_fn_caches */
static void cfn_prune(double now) {
    /* expire TTL-stale verdicts */
    for (size_t i = 0; i < g_cfn_count; i++) {
        if (now - g_cfn_cache[i].ts >= CHECK_FN_TTL_SECONDS) {
            /* invalidate this entry */
            g_cfn_cache[i].ts = 0; g_cfn_cache[i].value = false;
            free(g_cfn_cache[i].scope);
            g_cfn_cache[i].scope = NULL; g_cfn_cache[i].fn = NULL;
        }
    }
    /* expire last-good grace entries */
    for (size_t i = 0; i < g_cfn_count; i++) {
        if (g_cfn_cache[i].last_good_valid &&
            now - g_cfn_cache[i].last_good_ts >= CHECK_FN_FAILURE_GRACE_SECONDS) {
            g_cfn_cache[i].last_good_valid = false;
        }
    }
    /* compact: drop fully-invalidated entries */
    size_t w = 0;
    for (size_t i = 0; i < g_cfn_count; i++) {
        if (g_cfn_cache[i].fn != NULL) g_cfn_cache[w++] = g_cfn_cache[i];
    }
    g_cfn_count = w;
    /* cap at max (FIFO eviction: drop from front) */
    while (g_cfn_count >= (size_t)CHECK_FN_CACHE_MAX && g_cfn_count > 0) {
        free(g_cfn_cache[0].scope);
        for (size_t i = 1; i < g_cfn_count; i++)
            g_cfn_cache[i-1] = g_cfn_cache[i];
        g_cfn_count--;
    }
}

/* ── get_cached_check_fn_result ────────────────────────────────────── */
/* PoP: get_cached_check_fn_result @ tools/registry.py:get_cached_check_fn_result */
/* Return the current cached verdict for *fn* if its TTL is still valid.
 * NEVER executes the probe.  For read-only surfaces.  Returns: default_value
 * when no fresh verdict (hit=false), cached value when hit=true. */
bool get_cached_check_fn_result(void *fn, bool *hit, bool default_value) {
    double now = cfn_now();
    char *scope = check_fn_cache_scope();
    bool result = default_value;
    bool found = false;

    /* CHECK_FN_CACHE_BYPASS is the empty string "": unresolved multiplex
     * profile identity → no trustworthy cached verdict → return default.
     * NULL scope means process-wide cache (active). */
    if (scope && *scope == '\0') {
        free(scope);
        *hit = false; return default_value;
    }
    pthread_mutex_lock(&g_cfn_lock);
    for (size_t i = 0; i < g_cfn_count; i++) {
        if (g_cfn_cache[i].fn == fn &&
            scope_match(scope, g_cfn_cache[i].scope)) {
            if (now - g_cfn_cache[i].ts < CHECK_FN_TTL_SECONDS) {
                result = g_cfn_cache[i].value;
                found = true;
            }
            break;
        }
    }
    pthread_mutex_unlock(&g_cfn_lock);
    free(scope);
    *hit = found;
    return result;
}

/* ── cache write + read-for-call path (used by _check_fn_cached parity) ─ */
/* PoP: _check_fn_cached @ tools/registry.py:_check_fn_cached */
bool check_fn_cached(bool (*fn)(void)) {
    double now = cfn_now();
    char *scope = check_fn_cache_scope();
    /* BYPASS (empty string): probe without caching, swallow errors. */
    if (scope && *scope == '\0') {
        bool v = fn ? fn() : false;
        free(scope);
        return v;
    }
    pthread_mutex_lock(&g_cfn_lock);
    cfn_prune(now);

    /* look for existing entry (scope NULL == process-wide) */
    for (size_t i = 0; i < g_cfn_count; i++) {
        if (g_cfn_cache[i].fn == (void*)fn &&
            scope_match(scope, g_cfn_cache[i].scope)) {
            if (now - g_cfn_cache[i].ts < CHECK_FN_TTL_SECONDS) {
                bool v = g_cfn_cache[i].value;
                pthread_mutex_unlock(&g_cfn_lock);
                free(scope);
                return v;
            }
            break;
        }
    }

    /* ensure capacity */
    if (g_cfn_count >= g_cfn_cap) {
        size_t ncap = g_cfn_cap ? g_cfn_cap * 2 : CFN_CACHE_INIT;
        cfn_entry_t *na = realloc(g_cfn_cache, ncap * sizeof(*na));
        if (!na) {
            pthread_mutex_unlock(&g_cfn_lock);
            free(scope);
            bool v = fn ? fn() : false;
            return v;
        }
        g_cfn_cache = na; g_cfn_cap = ncap;
    }

    bool raised = false;
    bool value = false;
    if (fn) {
        value = fn();
    } else {
        raised = true; value = false;
    }

    /* last-good grace: if we have a recent True and this run was False,
     * return the True without caching the False. */
    for (size_t i = 0; i < g_cfn_count; i++) {
        if (g_cfn_cache[i].fn == (void*)fn &&
            scope_match(scope, g_cfn_cache[i].scope)) {
            if (g_cfn_cache[i].last_good_valid &&
                g_cfn_cache[i].value &&
                now - g_cfn_cache[i].last_good_ts < CHECK_FN_FAILURE_GRACE_SECONDS &&
                !value && !raised) {
                /* suppress transient failure, keep stale True */
                pthread_mutex_unlock(&g_cfn_lock);
                free(scope);
                return g_cfn_cache[i].value;
            }
            break;
        }
    }

    /* cache the verdict */
    cfn_entry_t *e = &g_cfn_cache[g_cfn_count++];
    e->fn = (void*)fn;
    e->scope = scope ? strdup(scope) : NULL;
    e->ts = now;
    e->value = value;
    if (value && !raised) {
        e->last_good_ts = now;
        e->last_good_valid = true;
    } else {
        e->last_good_valid = false;
    }

    pthread_mutex_unlock(&g_cfn_lock);
    free(scope);
    return value;
}

/* PoP: invalidate_check_fn_cache @ tools/registry.py:invalidate_check_fn_cache */
void invalidate_check_fn_cache(void) {
    pthread_mutex_lock(&g_cfn_lock);
    for (size_t i = 0; i < g_cfn_count; i++) {
        free(g_cfn_cache[i].scope);
    }
    g_cfn_count = 0;
    pthread_mutex_unlock(&g_cfn_lock);
}
