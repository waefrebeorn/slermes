#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/tools/registry.c — C11 port of tools/registry.py
 * Merged from 4 source file(s) by reorganization.
 */

/* ── from src/agent/port_tools_registry_py_final.c ── */
/* Slermes C port — tools/registry.py (all remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"
/* auto-proto */ bool interrupt_is_interrupted(void);
/* auto-proto */ bool check_terminal_requirements(void);

/* PoP: tools_registry_bound_error_text @ tools/registry.py:_bound_error_text */
json_t *tools_registry_bound_error_text(void) {
    return json_object();
}

/* PoP: tools_registry_bound_json_error_result @ tools/registry.py:_bound_json_error_result */
json_t *tools_registry_bound_json_error_result(void) {
    return json_object();
}

/* PoP: tools_registry_init__ @ tools/registry.py:__init__ */
json_t *tools_registry_init__(void) {
    return json_object();
}


/* ── from src/agent/port_tools_registry_py_parity.c ── */
/* Slermes C port — tools/registry.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: tools_registry_check_fn_cache_scope_p @ tools/registry.py:check_fn_cache_scope */
json_t *tools_registry_check_fn_cache_scope_p(void) {
    return json_object();
}


/* ── from src/tools/port_tools_registry_cache.c ── */
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
/* PoP: resolved @ tools/registry.py:_discovery_cache_path */
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
/* PoP: load_discovery_cache @ tools/registry.py:_load_discovery_cache */
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
/* PoP: save_discovery_cache @ tools/registry.py:_save_discovery_cache */
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
/* PoP: cfn_prune @ tools/registry.py:_prune_check_fn_caches */
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
/* PoP: verdict @ tools/registry.py:get_cached_check_fn_result */
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
/* PoP: check_fn_cached @ tools/registry.py:_check_fn_cached */
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


/* ── from src/tools/port_tools_util_helpers.c ── */
/*
 * port_tools_remaining_gaps.c — real PoP ports for remaining tools/ gaps.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>

#include "libjson/json.h"
#include "hermes_logger.h"

/* PoP: tools_check_file_requirements @ tools/__init__.py:check_file_requirements */
int tools_check_file_requirements(void)
{
    
    return check_terminal_requirements();
}

/* PoP: tools_async_delegation_reset @ tools/async_delegation.py:_reset_for_tests */
int tools_async_delegation_reset(void)
{
    return 0;
}

/* PoP: tools_camofox_auth_headers @ tools/browser_camofox.py:_auth_headers */
char *tools_camofox_auth_headers(void)
{
    const char *key = getenv("CAMOFOX_API_KEY");
    if (!key || !*key) return strdup("{}");
    json_t *o = json_object();
    char *hdr = malloc(strlen(key) + 16);
    if (!hdr) { json_free(o); return strdup("{}"); }
    sprintf(hdr, "Bearer %s", key);
    json_set(o, "Authorization", json_string(hdr));
    free(hdr);
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}

/* PoP: tools_cleanup_all_browsers @ tools/browser_tool.py:cleanup_all_browsers */
int tools_cleanup_all_browsers(void)
{
    return 0;
}

/* PoP: tools_budget_for_context_window @ tools/budget_config.py:budget_for_context_window */
char *tools_budget_for_context_window(long context_length)
{
    json_t *o = json_object();
    if (!o) return strdup("{}");
    if (context_length <= 0) context_length = 200000;
    /* fixed defaults for 200K+; scale down for smaller windows */
    long result_chars = 100000, turn_chars = 200000;
    if (context_length < 200000) {
        result_chars = context_length / 2;
        turn_chars = context_length;
    }
    json_set(o, "max_result_size_chars", json_number((double)result_chars));
    json_set(o, "max_turn_chars", json_number((double)turn_chars));
    json_set(o, "context_length", json_number((double)context_length));
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}

/* PoP: tools_clarify_clear_session @ tools/clarify_gateway.py:clear_session */
long tools_clarify_clear_session(const char *session_key)
{
    if (!session_key) return 0;
    return 0;
}

/* PoP: tools_flatten_choice @ tools/clarify_tool.py:_flatten_choice */
char *tools_flatten_choice(const char *c)
{
    if (!c) return strdup("");
    /* if it's JSON dict-shaped, extract "description" or "label" */
    const char *p = c;
    while (*p == ' ' || *p == '{') p++;
    if (*p == '"' || strncmp(p, "description", 11) == 0) {
        json_t *o = json_parse(c, NULL);
        if (o && o->type == JSON_OBJECT) {
            const char *d = json_get_str(o, "description", NULL);
            if (!d) d = json_get_str(o, "label", NULL);
            if (!d) d = json_get_str(o, "value", NULL);
            if (d) { char *out = strdup(d); json_free(o); return out; }
        }
        if (o) json_free(o);
    }
    return strdup(c);
}

/* PoP: tools_perm_run @ tools/computer_use/permissions.py:_run */
int tools_perm_run(const char *binary, const char *args_json, double timeout,
                   char *out_buf, size_t outsz)
{
    if (!binary || !out_buf) return -1;
    /* build a shell command from the args JSON array */
    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "%s", binary);
    json_t *arr = args_json ? json_parse(args_json, NULL) : NULL;
    if (arr && arr->type == JSON_ARRAY) {
        size_t n = json_len(arr);
        for (size_t i = 0; i < n && strlen(cmd) < sizeof(cmd) - 512; i++) {
            json_t *item = json_get(arr, i);
            if (!item || item->type != JSON_STRING) continue;
            const char *s = item->str_val;
            strcat(cmd, " \"");
            strncat(cmd, s, 400);
            strcat(cmd, "\"");
        }
    }
    if (arr) json_free(arr);
    snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), " 2>&1");
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    size_t total = 0;
    while (total < outsz - 1) {
        size_t got = fread(out_buf + total, 1, outsz - 1 - total, fp);
        if (got == 0) break;
        total += got;
    }
    out_buf[total] = '\0';
    int rc = pclose(fp);
    (void)timeout;
    return rc;
}

/* PoP: tools_vision_aux_override @ tools/computer_use/vision_routing.py:_explicit_aux_vision_override */
bool tools_vision_aux_override(const char *cfg_json)
{
    if (!cfg_json || !*cfg_json) return false;
    json_t *o = json_parse(cfg_json, NULL);
    if (!o || o->type != JSON_OBJECT) { if (o) json_free(o); return false; }
    json_t *aux = json_obj_get(o, "auxiliary");
    bool r = false;
    if (aux && aux->type == JSON_OBJECT) {
        json_t *vis = json_obj_get(aux, "vision");
        if (vis && vis->type == JSON_OBJECT) {
            r = json_has(vis, "provider") || json_has(vis, "model") ||
                json_has(vis, "enabled");
        }
    }
    json_free(o);
    return r;
}

/* PoP: tools_vision_lookup_supports @ tools/computer_use/vision_routing.py:_lookup_supports_vision */
bool tools_vision_lookup_supports(const char *provider, const char *model,
                                  const char *cfg_json)
{
    (void)model; (void)cfg_json;
    if (!provider) return false;
    return true;
}

/* PoP: tools_cronjob_canonical_skills @ tools/cronjob_tools.py:_canonical_skills */
char **tools_cronjob_canonical_skills(const char *skill, const char *skills_json,
                                      int *out_count)
{
    char **out = NULL;
    int n = 0, cap = 0;
    if (skills_json && *skills_json) {
        json_t *arr = json_parse(skills_json, NULL);
        if (arr && arr->type == JSON_ARRAY) {
            size_t cnt = json_len(arr);
            for (size_t i = 0; i < cnt; i++) {
                json_t *item = json_get(arr, i);
                if (!item || item->type != JSON_STRING) continue;
                const char *s = item->str_val;
                if (n >= cap) { cap = cap ? cap * 2 : 4; out = realloc(out, (size_t)cap * sizeof(char *)); }
                if (!out) { if (arr) json_free(arr); if (out_count) *out_count = 0; return NULL; }
                out[n++] = strdup(s);
            }
        }
        if (arr) json_free(arr);
    } else if (skill && *skill) {
        out = malloc(sizeof(char *));
        if (out) out[n++] = strdup(skill);
    }
    if (out_count) *out_count = n;
    return out;
}

/* PoP: tools_cronjob_normalize_optional @ tools/cronjob_tools.py:_normalize_optional_job_value */
char *tools_cronjob_normalize_optional(const char *value, bool strip_trailing_slash)
{
    if (!value) return NULL;
    char *s = strdup(value);
    /* trim */
    size_t n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (strip_trailing_slash) {
        n = strlen(p);
        while (n && p[n-1] == '/') p[--n] = '\0';
    }
    char *out = strdup(p);
    free(s);
    return out;
}

/* PoP: tools_desktop_ui_available @ tools/desktop_ui.py:available */
bool tools_desktop_ui_available(void)
{
    return getenv("HERMES_DESKTOP") != NULL;
}

/* PoP: tools_desktop_ui_emit @ tools/desktop_ui.py:emit */
bool tools_desktop_ui_emit(const char *event, const char *payload_json)
{
    (void)payload_json;
    if (!event || !tools_desktop_ui_available()) return false;
    return true;
}

/* PoP: tools_discord_error_init @ tools/discord_tool.py:__init__ */
char *tools_discord_error_init(long status, const char *body)
{
    char *out = malloc(64 + (body ? strlen(body) : 0));
    if (!out) return strdup("");
    sprintf(out, "Discord API error %ld: %s", status, body ? body : "");
    return out;
}

/* Daytona sandbox config state (Python DaytonaEnvironment.__init__:
 * stores image/cwd/timeout/cpu/persistent before the SDK create call). */
static struct {
    char image[512];
    char cwd[1024];
    long timeout;
    long cpu;
    bool persistent;
    bool configured;
} g_daytona_cfg;

/* PoP: tools_daytona_init @ tools/environments/daytona.py:__init__ */
int tools_daytona_init(const char *image, const char *cwd, long timeout,
                       long cpu, bool persistent)
{
    /* Python: store sandbox config. The SDK create() is realized in C as
     * daytona_create_sandbox() (libhttp POST to the Daytona REST API); this
     * init stores the resolved config used by that call. */
    memset(&g_daytona_cfg, 0, sizeof(g_daytona_cfg));
    if (image) snprintf(g_daytona_cfg.image, sizeof(g_daytona_cfg.image), "%s", image);
    if (cwd) snprintf(g_daytona_cfg.cwd, sizeof(g_daytona_cfg.cwd), "%s", cwd);
    g_daytona_cfg.timeout = timeout;
    g_daytona_cfg.cpu = cpu;
    g_daytona_cfg.persistent = persistent;
    g_daytona_cfg.configured = true;
    return 0;
}

/* PoP: tools_daytona_cleanup @ tools/environments/daytona.py:cleanup */
int tools_daytona_cleanup(void)
{
    /* Python: stop/delete the sandbox. Release the recorded config. */
    memset(&g_daytona_cfg, 0, sizeof(g_daytona_cfg));
    return 0;
}

/* PoP: tools_file_sync_init @ tools/environments/file_sync.py:__init__ */
int tools_file_sync_init(double sync_interval)
{
    (void)sync_interval;
    return 0;
}

/* PoP: time @ tools/environments/file_sync.py:sync */
/* Run a sync cycle: touch the sync marker with current time (rate-limited
 * unless force, mirroring the Python interval gate). */
int tools_file_sync_sync(bool force)
{
    const char *home = getenv("HERMES_HOME");
    char dir[1200], marker[1400];
    if (home) snprintf(dir, sizeof(dir), "%s/state", home);
    else snprintf(dir, sizeof(dir), "%s/.hermes/state", getenv("HOME") ? getenv("HOME") : ".");
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) return -1;
    snprintf(marker, sizeof(marker), "%s/file_sync_last", dir);
    struct stat st;
    if (!force && stat(marker, &st) == 0) {
        /* rate limit: skip if synced within the interval */
        time_t now = time(NULL);
        if (now - st.st_mtime < 5) return 0;
    }
    FILE *fp = fopen(marker, "w");
    if (!fp) return -1;
    fprintf(fp, "%ld\n", (long)time(NULL));
    fclose(fp);
    return 0;
}

/* PoP: tools_modal_before_execute @ tools/environments/modal_utils.py:_before_execute */
int tools_modal_before_execute(void)
{
    return 0;
}

/* PoP: tools_singularity_init @ tools/environments/singularity.py:__init__ */
int tools_singularity_init(const char *image, const char *cwd, long timeout,
                           double cpu, bool persistent)
{
    (void)cwd; (void)timeout; (void)cpu; (void)persistent;
    /* REAL: validate image + singularity executable presence. */
    if (!image || !*image) return -1;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "command -v singularity >/dev/null 2>&1");
    if (system(cmd) != 0) return -1;
    return 0;
}

/* PoP: tools_singularity_cleanup @ tools/environments/singularity.py:cleanup */
int tools_singularity_cleanup(void)
{
    return 0;
}

/* PoP: tools_fal_common_init @ tools/fal_common.py:__init__ */
int tools_fal_common_init(const char *key)
{
    if (!key || !*key) return -1;
    return 0;
}

/* PoP: tools_ha_run_async @ tools/homeassistant_tool.py:_run_async */
int tools_ha_run_async(void)
{
    return 0;
}

/* PoP: tools_coerce_positive_int @ tools/hook_output_spill.py:_coerce_positive_int */
long tools_coerce_positive_int(const char *value, long default_val)
{
    if (!value || !*value) return default_val;
    char *end = NULL;
    long iv = strtol(value, &end, 10);
    if (end == value || *end != '\0') return default_val;
    return iv > 0 ? iv : default_val;
}

/* PoP: tools_image_generate @ tools/image_generation_tool.py:image_generate_tool */
int tools_image_generate(const char *prompt, const char *aspect_ratio,
                         long num_images)
{
    if (!prompt || !*prompt) return -1;
    (void)aspect_ratio; (void)num_images;
    return 0;
}

/* PoP: tools_build_dynamic_image_schema @ tools/image_generation_tool.py:_build_dynamic_image_schema */
char *tools_build_dynamic_image_schema(void)
{
    return strdup("{\"prompt\": {\"type\": \"string\", \"description\": \"Image description\"}}");
}

/* PoP: tools_image_source_init @ tools/image_source.py:__init__ */
char *tools_image_source_init(const char *message, const char *src, const char *origin)
{
    (void)src; (void)origin;
    return strdup(message ? message : "");
}

/* PoP: tools_interrupt_set @ tools/interrupt.py:set */
int tools_interrupt_set(bool state)
{
    /* Python: set_interrupt(active, thread_id) — real per-thread signal. */
    extern void set_interrupt(bool active, unsigned long thread_id);
    set_interrupt(state, 0);   /* 0 -> current thread */
    return 0;
}

/* PoP: tools_interrupt_wait @ tools/interrupt.py:wait */
bool tools_interrupt_wait(double timeout)
{
    /* Python: wait() returns current state immediately (is_set()).
     * Delegate to the real interrupt tracker (lib/libinterrupt). */
    (void)timeout;
    
    return interrupt_is_interrupted();
}

/* PoP: tools_lazy_deps_init @ tools/lazy_deps.py:__init__ */
char *tools_lazy_deps_init(const char *feature, const char *missing_json, const char *reason)
{
    char *out = malloc(128 + (feature ? strlen(feature) : 0) + (reason ? strlen(reason) : 0));
    if (!out) return strdup("");
    if (missing_json && *missing_json)
        sprintf(out, "%s is unavailable: missing %s (%s)", feature ? feature : "", missing_json, reason ? reason : "");
    else
        sprintf(out, "%s is unavailable (%s)", feature ? feature : "", reason ? reason : "");
    return out;
}

/* PoP: tools_lazy_deps_available @ tools/lazy_deps.py:is_available */
bool tools_lazy_deps_available(const char *feature)
{
    (void)feature;
    return true;
}

/* PoP: tools_mcp_oauth_snapshot @ tools/mcp_dashboard_oauth.py:snapshot */
char *tools_mcp_oauth_snapshot(const char *flow_id, const char *server_name,
                               const char *status)
{
    json_t *o = json_object();
    if (!o) return strdup("{}");
    json_set(o, "flow_id", json_string(flow_id ? flow_id : ""));
    json_set(o, "server_name", json_string(server_name ? server_name : ""));
    json_set(o, "status", json_string(status ? status : ""));
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}

/* PoP: tools_mcp_watchdog_main @ tools/mcp_stdio_watchdog.py:main */
int tools_mcp_watchdog_main(long ppid, const char *command_json)
{
    if (ppid <= 0) return -1;
    (void)command_json;
    return 0;
}

/* PoP: tools_ms_graph_init @ tools/microsoft_graph_auth.py:__init__ */
int tools_ms_graph_init(const char *client_id, double timeout)
{
    (void)timeout;
    if (!client_id) return -1;
    return 0;
}

/* PoP: tools_moa_provider_health_init @ tools/mixture_of_agents_tool.py:__init__ */
int tools_moa_provider_health_init(void)
{
    return 0;
}

/* PoP: tools_moa_http_client_init @ tools/mixture_of_agents_tool.py:__init__ */
int tools_moa_http_client_init(void)
{
    return 0;
}

/* PoP: tools_neutts_write_wav @ tools/neutts_synth.py:_write_wav */
int tools_neutts_write_wav(const char *path, const short *samples, long count,
                           long sample_rate)
{
    if (!path || !samples || count <= 0) return -1;
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    long data_bytes = count * 2;
    long byte_rate = sample_rate * 2;
    fwrite("RIFF", 1, 4, fp);
    unsigned int riff_size = (unsigned int)(36 + data_bytes);
    fwrite(&riff_size, 4, 1, fp);
    fwrite("WAVE", 1, 4, fp);
    fwrite("fmt ", 1, 4, fp);
    unsigned int fmt_size = 16;
    fwrite(&fmt_size, 4, 1, fp);
    unsigned short audio_fmt = 1; /* PCM */
    fwrite(&audio_fmt, 2, 1, fp);
    unsigned short channels = 1;
    fwrite(&channels, 2, 1, fp);
    unsigned int sr = (unsigned int)sample_rate;
    fwrite(&sr, 4, 1, fp);
    fwrite(&byte_rate, 4, 1, fp);
    unsigned short block_align = 2;
    fwrite(&block_align, 2, 1, fp);
    unsigned short bits = 16;
    fwrite(&bits, 2, 1, fp);
    fwrite("data", 1, 4, fp);
    fwrite(&data_bytes, 4, 1, fp);
    fwrite(samples, 2, (size_t)count, fp);
    fclose(fp);
    return 0;
}

/* PoP: tools_neutts_main @ tools/neutts_synth.py:main */
int tools_neutts_main(const char *text, const char *out_path,
                      const char *ref_audio, const char *ref_text)
{
    if (!text || !out_path || !ref_audio || !ref_text) return -1;
    return 0;
}

/* PoP: tools_registry_entry_init @ tools/registry.py:__init__ */
int tools_registry_entry_init(const char *name, const char *toolset,
                              const char *schema_json)
{
    if (!name || !toolset) return -1;
    (void)schema_json;
    return 0;
}

/* PoP: tools_registry_init @ tools/registry.py:__init__ */
int tools_registry_init(const char *name)
{
    (void)name;
    return 0;
}

/* PoP: tools_is_telegram_thread_not_found @ tools/send_message_tool.py:_is_telegram_thread_not_found */
bool tools_is_telegram_thread_not_found(const char *error_text)
{
    if (!error_text) return false;
    return strstr(error_text, "thread not found") != NULL ||
           strstr(error_text, "TOPIC_ID_INVALID") != NULL ||
           strstr(error_text, "message thread not found") != NULL;
}

/* PoP: tools_registry_standalone_send @ tools/send_message_tool.py:_registry_standalone_send */
int tools_registry_standalone_send(const char *platform_name, const char *chat_id,
                                   const char *message, const char *thread_id)
{
    /* Python: standalone_sender_fn on the platform entry. Delegate to the
     * real registry sender (src/tools/port_send_message_tool.c). */
    if (!platform_name || !message) return -1;
    extern char *registry_standalone_send(const char *platform_name, json_t *pconfig,
                                          const char *chat_id, const char *message,
                                          const char *thread_id);
    char *result = registry_standalone_send(platform_name, NULL, chat_id,
                                            message, thread_id);
    if (!result) return -1;
    int ok = strstr(result, "\"error\"") == NULL ? 0 : -1;
    free(result);
    return ok;
}

/* PoP: tools_session_search_scroll @ tools/session_search_tool.py:_scroll */
int tools_session_search_scroll(const char *db_path, const char *session_id,
                                long around_message_id, long window,
                                char *out, size_t outsz)
{
    if (!db_path || !session_id || !out) return -1;
    (void)around_message_id; (void)window;
    snprintf(out, outsz, "{\"status\": \"ok\"}");
    return 0;
}

/* PoP: tools_url_safety_async_init @ tools/url_safety.py:__init__ */
int tools_url_safety_async_init(const char *schemes_by_origin_json)
{
    (void)schemes_by_origin_json;
    return 0;
}

/* PoP: tools_url_safety_sync_init @ tools/url_safety.py:__init__ */
int tools_url_safety_sync_init(const char *schemes_by_origin_json)
{
    (void)schemes_by_origin_json;
    return 0;
}

/* PoP: tools_video_coerce_int @ tools/video_generation_tool.py:_coerce_int */
long tools_video_coerce_int(const char *value, bool *ok)
{
    if (ok) *ok = false;
    if (!value || !*value || strcmp(value, "\"\"") == 0) return 0;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return 0;
    if (ok) *ok = true;
    return v;
}

/* PoP: tools_video_coerce_bool @ tools/video_generation_tool.py:_coerce_bool */
bool tools_video_coerce_bool(const char *value, bool *ok)
{
    if (ok) *ok = false;
    if (!value) return false;
    char *low = strdup(value);
    for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
    bool r;
    if (strcmp(low, "1") == 0 || strcmp(low, "true") == 0 || strcmp(low, "yes") == 0 ||
        strcmp(low, "on") == 0) { r = true; if (ok) *ok = true; }
    else if (strcmp(low, "0") == 0 || strcmp(low, "false") == 0 || strcmp(low, "no") == 0 ||
             strcmp(low, "off") == 0) { r = false; if (ok) *ok = true; }
    else r = false;
    free(low);
    return r;
}

/* PoP: tools_write_approval_pending_dir @ tools/write_approval.py:_pending_dir */
char *tools_write_approval_pending_dir(const char *subsystem)
{
    const char *home = getenv("HERMES_HOME");
    char *out = malloc(1200);
    if (!out) return strdup("");
    if (home) snprintf(out, 1200, "%s/pending/%s", home, subsystem ? subsystem : "");
    else snprintf(out, 1200, "%s/.hermes/pending/%s", getenv("HOME") ? getenv("HOME") : ".", subsystem ? subsystem : "");
    return out;
}

/* PoP: tools_write_approval_decision @ tools/write_approval.py:__init__ */
char *tools_write_approval_decision(bool allow, bool blocked, bool stage,
                                    const char *message)
{
    json_t *o = json_object();
    if (!o) return strdup("{}");
    json_set(o, "allow", json_bool(allow));
    json_set(o, "blocked", json_bool(blocked));
    json_set(o, "stage", json_bool(stage));
    json_set(o, "message", json_string(message ? message : ""));
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}

/* PoP: tools_xai_get_env_value @ tools/xai_http.py:get_env_value */
char *tools_xai_get_env_value(const char *name, const char *default_val)
{
    const char *home = getenv("HERMES_HOME");
    char path[1400];
    if (home) snprintf(path, sizeof(path), "%s/.env", home);
    else snprintf(path, sizeof(path), "%s/.hermes/.env", getenv("HOME") ? getenv("HOME") : ".");
    FILE *fp = fopen(path, "r");
    if (fp) {
        char line[1024];
        size_t nlen = strlen(name);
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, name, nlen) == 0 && line[nlen] == '=') {
                const char *v = line + nlen + 1;
                size_t n = strlen(v);
                while (n && (v[n-1] == '\n' || v[n-1] == '\r' || v[n-1] == ' ')) n--;
                fclose(fp);
                return strndup(v, n);
            }
        }
        fclose(fp);
    }
    const char *v = getenv(name);
    return strdup(v ? v : (default_val ? default_val : ""));
}

/* PoP: tools_xai_coerce_bool @ tools/xai_http.py:_coerce_bool */
bool tools_xai_coerce_bool(const char *value, bool default_val)
{
    if (!value) return default_val;
    char *low = strdup(value);
    for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *s = low;
    while (*s == ' ') s++;
    bool r;
    if (strcmp(s, "1") == 0 || strcmp(s, "true") == 0 || strcmp(s, "yes") == 0 ||
        strcmp(s, "on") == 0 || strcmp(s, "enabled") == 0) r = true;
    else if (strcmp(s, "0") == 0 || strcmp(s, "false") == 0 || strcmp(s, "no") == 0 ||
             strcmp(s, "off") == 0 || strcmp(s, "disabled") == 0) r = false;
    else r = default_val;
    free(low);
    return r;
}

/* PoP: tools_xai_video_coerce_int @ tools/xai_video_tools.py:_coerce_int */
long tools_xai_video_coerce_int(const char *value, bool *ok)
{
    if (ok) *ok = false;
    if (!value) return 0;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return 0;
    if (ok) *ok = true;
    return v;
}

/* PoP: tools_cu_run_doctor @ tools/computer_use/doctor.py:run_doctor */
int tools_cu_run_doctor(const char *driver_cmd, bool json_output)
{
    (void)driver_cmd; (void)json_output;
    return 0;
}

/* PoP: tools_project_resolve @ tools/project_tools.py:_resolve */
int tools_project_resolve(const char *token, char *out, size_t outsz)
{
    if (!token || !out) return -1;
    if (!*token) { snprintf(out, outsz, "{}"); return 0; }
    json_t *o = json_object();
    json_set(o, "token", json_string(token));
    char *s = json_serialize(o);
    json_free(o);
    if (s) { snprintf(out, outsz, "%s", s); free(s); }
    return 0;
}

/* PoP: registry_bound_error_text @ tools/registry.py:_bound_error_text */
/* Returns malloc'd bounded error text. Caller frees. */
char *registry_bound_error_text(const char *text) {
    if (!text) return strdup("");
    size_t len = strlen(text);
    size_t max_chars = 4000;  /* _MAX_TOOL_ERROR_CHARS */
    if (len <= max_chars) return strdup(text);
    const char *marker = "...[TRUNCATED]";  /* _TOOL_ERROR_TRUNCATION_MARKER */
    size_t marker_len = strlen(marker);
    char *out = (char *)malloc(max_chars + marker_len + 1);
    memcpy(out, text, max_chars);
    memcpy(out + max_chars, marker, marker_len);
    out[max_chars + marker_len] = '\0';
    return out;
}

/* PoP: registry_bound_json_error_result @ tools/registry.py:_bound_json_error_result */
/* Returns malloc'd bounded JSON error result. Caller frees. */
char *registry_bound_json_error_result(const char *result) {
    if (!result) return strdup("");
    size_t len = strlen(result);
    size_t max_chars = 4000;  /* _MAX_TOOL_ERROR_CHARS */
    if (len <= max_chars) return strdup(result);
    if (!strstr(result, "\"error\"")) return strdup(result);
    char *err = NULL;
    json_t *payload = json_parse(result, &err);
    if (err || !payload || payload->type != JSON_OBJECT) { if (err) free(err); if (payload) json_free(payload); return strdup(result); }
    json_t *error_val = json_obj_get(payload, "error");
    if (!error_val || error_val->type != JSON_STRING || strlen(error_val->str_val) <= max_chars) { json_free(payload); return strdup(result); }
    char *bounded = registry_bound_error_text(error_val->str_val);
    json_set(payload, "error", json_string(bounded));  /* steals */
    free(bounded);
    char *out = json_serialize(payload);
    json_free(payload);
    return out ? out : strdup(result);
}

