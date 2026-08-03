/*
 * port_auth_store.c — auth.json persistence layer ported from hermes_cli/auth.py.
 * See include/port_auth_store.h for the API surface. C11, opaque txn struct,
 * minimal includes. Reuses: auth_file_path/global_auth_file_path/has_usable_secret
 * (port_auth_na.c), auth_helpers (JWT/scope/expiry), provider_registry,
 * config_py_* (config.yaml IO), sanitize_borrowed_credential_payload,
 * _parse_absolute_timestamp/_exhausted_ttl (credential_pool internals).
 */
#include "port_auth_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <pthread.h>

#include "hermes_json.h"
#include "hermes_logger.h"
#include "port_provider_registry.h"

/* ── external reuse (port_auth_na.c) ── */
extern char *auth_file_path(void);           /* malloc'd <home>/auth.json */
extern char *global_auth_file_path(void);    /* malloc'd or NULL */
extern int   has_usable_secret(const char *value, int min_length);
/* port_config_py_io.c */
extern json_t *config_py_read_raw_config(void);
extern void    config_py_get_config_path(char *buf, size_t sz);
extern int     config_py_atomic_config_write(const char *config_path, const json_t *data);
extern int     config_py_require_readable_config_before_write(const char *config_path);
extern char   *config_py_get_env_value_prefer_dotenv(const char *key);
/* credential pool internals */
extern json_node_t *sanitize_borrowed_credential_payload(const json_node_t *payload, const char *provider_id);
extern double _parse_absolute_timestamp(const char *value);
extern int    _exhausted_ttl(int error_code);
/* provider alias resolution (port_provider_registry.c) */
extern char *provider_resolve_alias(const char *requested);

/* ════════════════════════════════════════════════════════════════
 *  File lock — cross-process flock with per-path reentrancy depth
 * ════════════════════════════════════════════════════════════════ */

typedef struct lock_slot {
    char path[PATH_MAX];
    int  fd;
    int  depth;
    struct lock_slot *next;
} lock_slot_t;

static __thread lock_slot_t *tls_locks = NULL;


static lock_slot_t *lock_slot_find(const char *path) {
    for (lock_slot_t *s = tls_locks; s; s = s->next)
        if (strcmp(s->path, path) == 0) return s;
    return NULL;
}

/* PoP: auth_file_lock_acquire @ hermes_cli/auth.py:_file_lock */
int auth_file_lock_acquire(const char *lock_path, double timeout_seconds)
{
    if (!lock_path || !*lock_path) return 0;
    lock_slot_t *slot = lock_slot_find(lock_path);
    if (slot && slot->depth > 0) { slot->depth++; return 1; }  /* reentrant */

    /* ensure parent dir exists */
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", lock_path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; if (dir[0]) mkdir(dir, 0700); }

    int fd = open(lock_path, O_RDWR | O_CREAT | O_APPEND, 0600);
    if (fd < 0) return 0;

    double deadline_s = timeout_seconds < 1.0 ? 1.0 : timeout_seconds;
    struct timespec start; clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) break;
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (double)(now.tv_sec - start.tv_sec) +
                         (double)(now.tv_nsec - start.tv_nsec) / 1e9;
        if (elapsed >= deadline_s) { close(fd); return 0; }
        struct timespec nap = {0, 50 * 1000 * 1000};  /* 50ms — Python sleeps 0.05 */
        nanosleep(&nap, NULL);
    }

    if (!slot) {
        slot = calloc(1, sizeof(*slot));
        if (!slot) { flock(fd, LOCK_UN); close(fd); return 0; }
        snprintf(slot->path, sizeof(slot->path), "%s", lock_path);
        slot->next = tls_locks; tls_locks = slot;
    }
    slot->fd = fd;
    slot->depth = 1;
    return 1;
}

/* PoP: auth_file_lock_release @ hermes_cli/auth.py:_file_lock */
void auth_file_lock_release(const char *lock_path)
{
    if (!lock_path) return;
    lock_slot_t *slot = lock_slot_find(lock_path);
    if (!slot || slot->depth <= 0) return;
    if (--slot->depth == 0) {
        flock(slot->fd, LOCK_UN);
        close(slot->fd);
        slot->fd = -1;
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Auth store lock / path helpers
 * ════════════════════════════════════════════════════════════════ */

/* PoP: authstore_lock_path @ hermes_cli/auth.py:_auth_lock_path */
char *authstore_lock_path(void)
{
    char *auth = auth_file_path();
    if (!auth) return NULL;
    size_t n = strlen(auth);
    /* replace trailing ".json" with ".lock" (with_suffix semantics) */
    char *out = malloc(n + 8);
    if (!out) { free(auth); return NULL; }
    strcpy(out, auth);
    char *dot = strrchr(out, '.');
    if (dot && !strchr(dot, '/')) strcpy(dot, ".lock");
    else strcat(out, ".lock");
    free(auth);
    return out;
}

/* PoP: authstore_same_path @ hermes_cli/auth.py:_same_path */
bool authstore_same_path(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    char ra[PATH_MAX], rb[PATH_MAX];
    const char *pa = realpath(a, ra) ? ra : a;
    const char *pb = realpath(b, rb) ? rb : b;
    return strcmp(pa, pb) == 0;
}

static char *lock_path_for(const char *auth_path)
{
    size_t n = strlen(auth_path);
    char *out = malloc(n + 8);
    if (!out) return NULL;
    strcpy(out, auth_path);
    char *dot = strrchr(out, '.');
    if (dot && !strchr(dot, '/')) strcpy(dot, ".lock");
    else strcat(out, ".lock");
    return out;
}

/* PoP: authstore_lock @ hermes_cli/auth.py:_auth_store_lock */
int authstore_lock(const char *target_path, double timeout_seconds)
{
    char *auth = target_path ? strdup(target_path) : auth_file_path();
    if (!auth) return 0;
    char *lp = lock_path_for(auth);
    free(auth);
    if (!lp) return 0;
    int ok = auth_file_lock_acquire(lp, timeout_seconds > 0 ? timeout_seconds
                                                            : AUTH_LOCK_TIMEOUT_SECONDS_C);
    free(lp);
    return ok;
}

/* PoP: authstore_unlock @ hermes_cli/auth.py:_auth_store_lock */
void authstore_unlock(const char *target_path)
{
    char *auth = target_path ? strdup(target_path) : auth_file_path();
    if (!auth) return;
    char *lp = lock_path_for(auth);
    free(auth);
    if (!lp) return;
    auth_file_lock_release(lp);
    free(lp);
}

/* ════════════════════════════════════════════════════════════════
 *  Auth store load / save
 * ════════════════════════════════════════════════════════════════ */

static json_t *empty_store(void)
{
    json_t *s = json_new_object();
    json_set(s, "version", json_int(AUTH_STORE_VERSION_C));
    json_set(s, "providers", json_new_object());
    return s;
}

static char *iso_now_utc(void)
{
    time_t t = time(NULL);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    char *buf = malloc(40);
    if (!buf) return NULL;
    strftime(buf, 40, "%Y-%m-%dT%H:%M:%S+00:00", &tmv);
    return buf;
}

/* PoP: authstore_load @ hermes_cli/auth.py:_load_auth_store */
json_t *authstore_load(const char *auth_file)
{
    char *path = auth_file ? strdup(auth_file) : auth_file_path();
    if (!path) return empty_store();

    struct stat st;
    if (stat(path, &st) != 0) { free(path); return empty_store(); }

    char *err = NULL;
    json_t *raw = json_parse_file(path, &err);
    if (!raw) {
        /* preserve corrupt copy at <path>.json.corrupt (best-effort) */
        char corrupt[PATH_MAX + 16];
        snprintf(corrupt, sizeof(corrupt), "%s.corrupt", path);
        FILE *in = fopen(path, "rb");
        if (in) {
            FILE *out = fopen(corrupt, "wb");
            if (out) {
                char buf[4096]; size_t n;
                while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
                fclose(out);
            }
            fclose(in);
        }
        hermes_log(LOG_WARNING, "auth",
                   "failed to parse %s (%s) — starting with empty store; corrupt copy at %s",
                   path, err ? err : "parse error", corrupt);
        free(err); free(path);
        return empty_store();
    }
    free(err); free(path);

    if (raw->type == JSON_OBJECT) {
        json_t *providers = json_obj_get(raw, "providers");
        json_t *pool = json_obj_get(raw, "credential_pool");
        if ((providers && providers->type == JSON_OBJECT) ||
            (pool && pool->type == JSON_OBJECT)) {
            if (!providers || providers->type != JSON_OBJECT)
                json_set(raw, "providers", json_new_object());
            providers = json_obj_get(raw, "providers");
            if (providers && providers->type == JSON_OBJECT)
                auth_migrate_stale_nous_portal_url(providers);
            return raw;
        }
        /* migrate from PR's "systems" format */
        json_t *systems = json_obj_get(raw, "systems");
        if (systems && systems->type == JSON_OBJECT) {
            json_t *store = empty_store();
            json_t *nous = json_obj_get(systems, "nous_portal");
            if (nous) {
                json_t *providers2 = json_obj_get(store, "providers");
                json_set(providers2, "nous", json_copy(nous));
                json_set(store, "active_provider", json_new_string("nous"));
            } else {
                json_set(store, "active_provider", json_null());
            }
            json_free(raw);
            return store;
        }
    }
    json_free(raw);
    return empty_store();
}

/* PoP: authstore_load_global @ hermes_cli/auth.py:_load_global_auth_store */
json_t *authstore_load_global(void)
{
    char *gpath = global_auth_file_path();
    if (!gpath) return json_new_object();
    struct stat st;
    if (stat(gpath, &st) != 0) { free(gpath); return json_new_object(); }
    json_t *store = authstore_load(gpath);
    free(gpath);
    if (!store) return json_new_object();
    return store;
}

/* Tighten parent dir to 0700 (mirror secure_parent_dir; refuse "/" and 1-level dirs). */
static void secure_parent_dir_c(const char *file_path)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", file_path);
    char *slash = strrchr(dir, '/');
    if (!slash || slash == dir) return;
    *slash = '\0';
    /* refuse top-level dirs like "/home" (only one slash) */
    const char *second = strchr(dir + 1, '/');
    if (!second) return;
    chmod(dir, 0700);
}

/* PoP: authstore_save @ hermes_cli/auth.py:_save_auth_store */
char *authstore_save(json_t *auth_store, const char *target_path)
{
    if (!auth_store) return NULL;
    char *path = target_path ? strdup(target_path) : auth_file_path();
    if (!path) return NULL;

    /* mkdir -p parent */
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; if (dir[0]) mkdir(dir, 0700); }
    secure_parent_dir_c(path);

    json_set(auth_store, "version", json_int(AUTH_STORE_VERSION_C));
    char *now = iso_now_utc();
    if (now) { json_set(auth_store, "updated_at", json_new_string(now)); free(now); }

    char *payload = json_serialize_pretty(auth_store, 2);
    if (!payload) { free(path); return NULL; }

    /* atomic O_EXCL 0600 tmp + rename (TOCTOU-safe; mirrors #19673/#21148) */
    char tmp[PATH_MAX + 64];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d.%lx", path, (int)getpid(),
             (unsigned long)time(NULL) ^ (unsigned long)(size_t)payload);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd < 0) { free(payload); free(path); return NULL; }
    size_t len = strlen(payload);
    ssize_t wr = write(fd, payload, len);
    if (wr == (ssize_t)len) wr += write(fd, "\n", 1);
    fsync(fd);
    close(fd);
    free(payload);
    if (wr != (ssize_t)(len + 1) || rename(tmp, path) != 0) {
        unlink(tmp); free(path); return NULL;
    }
    /* fsync parent dir */
    if (slash) {
        int dfd = open(dir, O_RDONLY);
        if (dfd >= 0) { fsync(dfd); close(dfd); }
    }
    chmod(path, S_IRUSR | S_IWUSR);
    return path;
}

/* ════════════════════════════════════════════════════════════════
 *  Provider state (profile -> global fallback)
 * ════════════════════════════════════════════════════════════════ */

/* PoP: authstore_load_provider_state_with_source @ hermes_cli/auth.py:_load_provider_state_with_source */
json_t *authstore_load_provider_state_with_source(json_t *auth_store, const char *provider_id,
                                                  char **out_source_path)
{
    if (out_source_path) *out_source_path = NULL;
    if (!auth_store || !provider_id) return NULL;

    json_t *providers = json_obj_get(auth_store, "providers");
    if (providers && providers->type == JSON_OBJECT) {
        json_t *state = json_obj_get(providers, provider_id);
        if (state && state->type == JSON_OBJECT) {
            if (out_source_path) *out_source_path = auth_file_path();
            return json_copy(state);
        }
    }

    char *gpath = global_auth_file_path();
    json_t *gstore = authstore_load_global();
    if (gstore) {
        json_t *gproviders = json_obj_get(gstore, "providers");
        if (gproviders && gproviders->type == JSON_OBJECT) {
            json_t *gstate = json_obj_get(gproviders, provider_id);
            if (gstate && gstate->type == JSON_OBJECT) {
                json_t *copy = json_copy(gstate);
                json_free(gstore);
                if (out_source_path) *out_source_path = gpath;
                else free(gpath);
                return copy;
            }
        }
        json_free(gstore);
    }
    free(gpath);
    return NULL;
}

/* PoP: authstore_load_provider_state @ hermes_cli/auth.py:_load_provider_state */
json_t *authstore_load_provider_state(json_t *auth_store, const char *provider_id)
{
    char *src = NULL;
    json_t *state = authstore_load_provider_state_with_source(auth_store, provider_id, &src);
    free(src);
    return state;
}

/* PoP: authstore_save_provider_state @ hermes_cli/auth.py:_save_provider_state */
void authstore_save_provider_state(json_t *auth_store, const char *provider_id, const json_t *state)
{
    if (!auth_store || !provider_id || !state) return;
    json_t *providers = json_obj_get(auth_store, "providers");
    if (!providers || providers->type != JSON_OBJECT) {
        json_set(auth_store, "providers", json_new_object());
        providers = json_obj_get(auth_store, "providers");
    }
    json_set(providers, provider_id, json_copy(state));
    json_set(auth_store, "active_provider", json_new_string(provider_id));
}

/* PoP: authstore_store_provider_state @ hermes_cli/auth.py:_store_provider_state */
void authstore_store_provider_state(json_t *auth_store, const char *provider_id,
                                    const json_t *state, bool set_active)
{
    if (!auth_store || !provider_id || !state) return;
    json_t *providers = json_obj_get(auth_store, "providers");
    if (!providers || providers->type != JSON_OBJECT) {
        json_set(auth_store, "providers", json_new_object());
        providers = json_obj_get(auth_store, "providers");
    }
    json_set(providers, provider_id, json_copy(state));
    if (set_active)
        json_set(auth_store, "active_provider", json_new_string(provider_id));
}

/* PoP: authstore_persist_provider_state_to_store @ hermes_cli/auth.py:_persist_provider_state_to_store */
char *authstore_persist_provider_state_to_store(const char *provider_id, const json_t *state,
                                                const char *target_path, bool set_active)
{
    if (!provider_id || !state || !target_path) return NULL;
    if (!authstore_lock(target_path, AUTH_LOCK_TIMEOUT_SECONDS_C)) return NULL;
    json_t *store = authstore_load(target_path);
    authstore_store_provider_state(store, provider_id, state, set_active);
    char *saved = authstore_save(store, target_path);
    json_free(store);
    authstore_unlock(target_path);
    return saved;
}

/* PoP: authstore_save_provider_state_to_source @ hermes_cli/auth.py:_save_provider_state_to_source */
void authstore_save_provider_state_to_source(json_t *auth_store, const char *provider_id,
                                             const json_t *state, const char *source_path)
{
    char *active = auth_file_path();
    bool same = true;
    if (source_path && active) same = authstore_same_path(source_path, active);
    free(active);
    if (same) {
        authstore_save_provider_state(auth_store, provider_id, state);
        char *saved = authstore_save(auth_store, NULL);
        free(saved);
        return;
    }
    char *saved = authstore_persist_provider_state_to_store(provider_id, state, source_path, true);
    free(saved);
}

/* ── opaque provider-state transaction ── */
struct auth_provider_txn {
    json_t *store;          /* active auth store (owned) */
    json_t *state;          /* provider state copy (owned, may be NULL) */
    char   *source_path;    /* owned, may be NULL */
    char   *locked_target;  /* second lock target (owned, may be NULL) */
};

/* PoP: authstore_provider_state_transaction @ hermes_cli/auth.py:_provider_state_transaction */
auth_provider_txn_t *authstore_provider_state_transaction(const char *provider_id)
{
    if (!provider_id) return NULL;
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return NULL;

    auth_provider_txn_t *txn = calloc(1, sizeof(*txn));
    if (!txn) { authstore_unlock(NULL); return NULL; }

    txn->store = authstore_load(NULL);
    txn->state = authstore_load_provider_state_with_source(txn->store, provider_id,
                                                           &txn->source_path);
    char *active = auth_file_path();
    bool same = (txn->source_path == NULL) ||
                (active && authstore_same_path(txn->source_path, active));
    free(active);
    if (same) return txn;

    /* profile read fell back to global: lock + re-read the source store */
    if (authstore_lock(txn->source_path, AUTH_LOCK_TIMEOUT_SECONDS_C)) {
        txn->locked_target = strdup(txn->source_path);
        json_t *sstore = authstore_load(txn->source_path);
        json_free(txn->state);
        txn->state = NULL;
        json_t *sproviders = json_obj_get(sstore, "providers");
        if (sproviders && sproviders->type == JSON_OBJECT) {
            json_t *raw = json_obj_get(sproviders, provider_id);
            if (raw && raw->type == JSON_OBJECT) txn->state = json_copy(raw);
        }
        json_free(sstore);
    }
    return txn;
}

json_t *auth_provider_txn_store(auth_provider_txn_t *txn) { return txn ? txn->store : NULL; }
json_t *auth_provider_txn_state(auth_provider_txn_t *txn) { return txn ? txn->state : NULL; }
const char *auth_provider_txn_source_path(auth_provider_txn_t *txn)
{ return txn ? txn->source_path : NULL; }

/* PoP: auth_provider_txn_end @ hermes_cli/auth.py:_provider_state_transaction */
void auth_provider_txn_end(auth_provider_txn_t *txn)
{
    if (!txn) return;
    if (txn->locked_target) { authstore_unlock(txn->locked_target); free(txn->locked_target); }
    authstore_unlock(NULL);
    json_free(txn->store);
    json_free(txn->state);
    free(txn->source_path);
    free(txn);
}

/* ════════════════════════════════════════════════════════════════
 *  Provider management
 * ════════════════════════════════════════════════════════════════ */

static void lc_copy(char *dst, size_t sz, const char *src)
{
    size_t i = 0;
    if (src) {
        /* strip leading/trailing whitespace, lowercase */
        while (*src && isspace((unsigned char)*src)) src++;
        size_t n = strlen(src);
        while (n > 0 && isspace((unsigned char)src[n-1])) n--;
        for (; i < n && i < sz - 1; i++)
            dst[i] = (char)tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

/* PoP: mark_provider_active_if_unset @ hermes_cli/auth.py:mark_provider_active_if_unset */
void mark_provider_active_if_unset(const char *provider_id)
{
    if (!provider_id || !*provider_id) return;
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return;
    json_t *store = authstore_load(NULL);
    const char *active = json_get_str(store, "active_provider", "");
    /* stripped emptiness check */
    bool unset = true;
    for (const char *p = active; p && *p; p++)
        if (!isspace((unsigned char)*p)) { unset = false; break; }
    if (unset) {
        json_set(store, "active_provider", json_new_string(provider_id));
        char *saved = authstore_save(store, NULL);
        free(saved);
    }
    json_free(store);
    authstore_unlock(NULL);
}

/* SERVICE_PROVIDER_NAMES (auth.py module constant): spotify only. */
static const struct { const char *id; const char *name; } SERVICE_PROVIDERS[] = {
    { "spotify", "Spotify" },
};
static const int SERVICE_PROVIDERS_N =
    (int)(sizeof(SERVICE_PROVIDERS)/sizeof(SERVICE_PROVIDERS[0]));

/* PoP: is_known_auth_provider @ hermes_cli/auth.py:is_known_auth_provider */
bool is_known_auth_provider(const char *provider_id)
{
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    if (!norm[0]) return false;
    if (provider_registry_get(norm)) return true;
    for (int i = 0; i < SERVICE_PROVIDERS_N; i++)
        if (strcmp(SERVICE_PROVIDERS[i].id, norm) == 0) return true;
    return false;
}

/* PoP: get_auth_provider_display_name @ hermes_cli/auth.py:get_auth_provider_display_name */
char *get_auth_provider_display_name(const char *provider_id)
{
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    const catalog_provider_t *pc = provider_registry_get(norm);
    if (pc && pc->name) return strdup(pc->name);
    for (int i = 0; i < SERVICE_PROVIDERS_N; i++)
        if (strcmp(SERVICE_PROVIDERS[i].id, norm) == 0)
            return strdup(SERVICE_PROVIDERS[i].name);
    return strdup(provider_id ? provider_id : "");
}

/* PoP: is_runtime_provider_routable @ hermes_cli/auth.py:is_runtime_provider_routable */
bool is_runtime_provider_routable(const char *provider_id)
{
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    if (!norm[0]) return false;
    if (strcmp(norm, "auto") == 0 || strcmp(norm, "openrouter") == 0 ||
        strcmp(norm, "custom") == 0 || strcmp(norm, "moa") == 0)
        return true;
    if (strncmp(norm, "custom:", 7) == 0) return true;
    /* resolve_provider(normalized) without env fallbacks: alias-resolve then
     * registry membership check (raises AuthError only for unknown ids). */
    char *resolved = provider_resolve_alias(norm);
    if (!resolved) return false;
    bool ok = (strcmp(resolved, "openrouter") == 0 || strcmp(resolved, "custom") == 0 ||
               provider_registry_get(resolved) != NULL);
    free(resolved);
    return ok;
}

/* PoP: read_credential_pool @ hermes_cli/auth.py:read_credential_pool */
json_t *read_credential_pool(const char *provider_id)
{
    json_t *store = authstore_load(NULL);
    json_t *pool = json_obj_get(store, "credential_pool");
    bool pool_ok = pool && pool->type == JSON_OBJECT;

    json_t *gstore = authstore_load_global();
    json_t *gpool = gstore ? json_obj_get(gstore, "credential_pool") : NULL;
    bool gpool_ok = gpool && gpool->type == JSON_OBJECT;

    json_t *result;
    if (!provider_id) {
        result = pool_ok ? json_copy(pool) : json_new_object();
        if (gpool_ok) {
            for (size_t i = 0; i < gpool->c.count; i++) {
                const char *key = gpool->c.keys[i];
                json_t *gentries = gpool->c.items[i];
                if (!gentries || gentries->type != JSON_ARRAY || gentries->c.count == 0)
                    continue;
                json_t *existing = json_obj_get(result, key);
                if (existing && existing->type == JSON_ARRAY && existing->c.count > 0)
                    continue;   /* profile shadows per-provider */
                json_set(result, key, json_copy(gentries));
            }
        }
    } else {
        json_t *entries = pool_ok ? json_obj_get(pool, provider_id) : NULL;
        if (entries && entries->type == JSON_ARRAY && entries->c.count > 0) {
            result = json_copy(entries);
        } else {
            json_t *gentries = gpool_ok ? json_obj_get(gpool, provider_id) : NULL;
            result = (gentries && gentries->type == JSON_ARRAY)
                     ? json_copy(gentries) : json_new_array();
        }
    }
    json_free(store);
    if (gstore) json_free(gstore);
    return result;
}

/* pool status fields adopted on merge (auth.py:_POOL_STATUS_FIELDS) */
static const char *POOL_STATUS_FIELDS[] = {
    "last_status", "last_status_at", "last_error_code",
    "last_error_reason", "last_error_message", "last_error_reset_at",
};
static const int POOL_STATUS_FIELDS_N =
    (int)(sizeof(POOL_STATUS_FIELDS)/sizeof(POOL_STATUS_FIELDS[0]));

/* dict-shaped _exhausted_until: reset_at wins, else last_status_at + ttl(code). */
static double pool_entry_exhausted_until(const json_t *disk_entry)
{
    const char *reset_at = json_get_str(disk_entry, "last_error_reset_at", NULL);
    if (reset_at && *reset_at) {
        double t = _parse_absolute_timestamp(reset_at);
        if (t > 0) return t;
    }
    double status_at = _parse_absolute_timestamp(
        json_get_str(disk_entry, "last_status_at", NULL));
    if (status_at > 0) {
        int code = (int)json_get_num(disk_entry, "last_error_code", 0);
        return status_at + (double)_exhausted_ttl(code);
    }
    return 0;
}

/* PoP: auth_merge_disk_cooldown_state @ hermes_cli/auth.py:_merge_disk_cooldown_state */
json_t *auth_merge_disk_cooldown_state(const json_t *entry, const json_t *disk_entry,
                                       const char *provider_id)
{
    (void)provider_id;
    if (!entry) return NULL;
    json_t *keep = json_copy(entry);
    if (!disk_entry || disk_entry->type != JSON_OBJECT) return keep;

    const char *disk_status = json_get_str(disk_entry, "last_status", NULL);
    if (!disk_status ||
        (strcmp(disk_status, "dead") != 0 && strcmp(disk_status, "exhausted") != 0))
        return keep;

    /* token change = caller re-authed; never resurrect the old cooldown */
    const char *mem_access = json_get_str(entry, "access_token", "");
    const char *disk_access = json_get_str(disk_entry, "access_token", "");
    if (mem_access[0] && disk_access[0] && strcmp(mem_access, disk_access) != 0)
        return keep;

    double disk_ts = _parse_absolute_timestamp(json_get_str(disk_entry, "last_status_at", NULL));
    double mem_ts = _parse_absolute_timestamp(json_get_str(entry, "last_status_at", NULL));
    if (disk_ts <= 0) disk_ts = 0;
    if (mem_ts <= 0) mem_ts = 0;
    if (disk_ts <= mem_ts) return keep;

    if (strcmp(disk_status, "exhausted") == 0) {
        double until = pool_entry_exhausted_until(disk_entry);
        if (until <= 0 || until <= (double)time(NULL)) return keep;
    }

    for (int i = 0; i < POOL_STATUS_FIELDS_N; i++) {
        json_t *v = json_obj_get(disk_entry, POOL_STATUS_FIELDS[i]);
        json_set(keep, POOL_STATUS_FIELDS[i], v ? json_copy(v) : json_null());
    }
    return keep;
}

static bool str_in_json_array(const json_t *arr, const char *s)
{
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *e = arr->c.items[i];
        if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, s) == 0)
            return true;
    }
    return false;
}

/* PoP: write_credential_pool @ hermes_cli/auth.py:write_credential_pool */
char *write_credential_pool(const char *provider_id, const json_t *entries,
                            const json_t *removed_ids)
{
    if (!provider_id || !entries || entries->type != JSON_ARRAY) return NULL;
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return NULL;

    json_t *store = authstore_load(NULL);
    json_t *pool = json_obj_get(store, "credential_pool");
    if (!pool || pool->type != JSON_OBJECT) {
        json_set(store, "credential_pool", json_new_object());
        pool = json_obj_get(store, "credential_pool");
    }

    json_t *existing = json_obj_get(pool, provider_id);
    bool existing_ok = existing && existing->type == JSON_ARRAY;

    json_t *merged = json_new_array();
    /* sanitize incoming entries; merge per-id cooldown state from disk */
    for (size_t i = 0; i < entries->c.count; i++) {
        json_t *entry = entries->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) {
            json_append(merged, entry ? json_copy(entry) : json_null());
            continue;
        }
        json_t *sanitized = sanitize_borrowed_credential_payload(entry, provider_id);
        if (!sanitized) sanitized = json_copy(entry);
        const char *id = json_get_str(sanitized, "id", NULL);
        json_t *disk_match = NULL;
        if (id && existing_ok) {
            for (size_t j = 0; j < existing->c.count; j++) {
                json_t *de = existing->c.items[j];
                if (de && de->type == JSON_OBJECT) {
                    const char *did = json_get_str(de, "id", NULL);
                    if (did && strcmp(did, id) == 0) { disk_match = de; break; }
                }
            }
        }
        json_t *final = auth_merge_disk_cooldown_state(sanitized, disk_match, provider_id);
        json_free(sanitized);
        json_append(merged, final);
    }

    /* re-adopt disk entries missing from the snapshot (added concurrently) */
    if (existing_ok) {
        for (size_t j = 0; j < existing->c.count; j++) {
            json_t *de = existing->c.items[j];
            if (!de || de->type != JSON_OBJECT) continue;
            const char *did = json_get_str(de, "id", NULL);
            if (!did || !*did) continue;
            if (removed_ids && str_in_json_array(removed_ids, did)) continue;
            bool present = false;
            for (size_t i = 0; i < merged->c.count; i++) {
                json_t *me = merged->c.items[i];
                if (me && me->type == JSON_OBJECT) {
                    const char *mid = json_get_str(me, "id", NULL);
                    if (mid && strcmp(mid, did) == 0) { present = true; break; }
                }
            }
            if (present) continue;
            json_t *sanitized = sanitize_borrowed_credential_payload(de, provider_id);
            json_append(merged, sanitized ? sanitized : json_copy(de));
        }
    }

    json_set(pool, provider_id, merged);
    char *saved = authstore_save(store, NULL);
    json_free(store);
    authstore_unlock(NULL);
    return saved;
}

/* PoP: unsuppress_credential_source @ hermes_cli/auth.py:unsuppress_credential_source */
bool unsuppress_credential_source(const char *provider_id, const char *source)
{
    if (!provider_id || !source) return false;
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return false;
    json_t *store = authstore_load(NULL);
    json_t *suppressed = json_obj_get(store, "suppressed_sources");
    bool cleared = false;
    if (suppressed && suppressed->type == JSON_OBJECT) {
        json_t *list = json_obj_get(suppressed, provider_id);
        if (list && list->type == JSON_ARRAY && str_in_json_array(list, source)) {
            /* rebuild the list without `source` */
            json_t *nl = json_new_array();
            for (size_t i = 0; i < list->c.count; i++) {
                json_t *e = list->c.items[i];
                if (e && e->type == JSON_STRING && e->str_val &&
                    strcmp(e->str_val, source) == 0)
                    continue;
                json_append(nl, e ? json_copy(e) : json_null());
            }
            if (nl->c.count == 0) {
                json_free(nl);
                json_obj_del(suppressed, provider_id);
                if (suppressed->c.count == 0)
                    json_obj_del(store, "suppressed_sources");
            } else {
                json_set(suppressed, provider_id, nl);
            }
            char *saved = authstore_save(store, NULL);
            free(saved);
            cleared = true;
        }
    }
    json_free(store);
    authstore_unlock(NULL);
    return cleared;
}

/* PoP: get_provider_auth_state @ hermes_cli/auth.py:get_provider_auth_state */
json_t *get_provider_auth_state(const char *provider_id)
{
    json_t *store = authstore_load(NULL);
    json_t *state = authstore_load_provider_state(store, provider_id);
    json_free(store);
    return state;
}

static bool moa_slot_matches(const json_t *slot, const char *norm)
{
    if (!slot || slot->type != JSON_OBJECT) return false;
    const char *p = json_get_str(slot, "provider", "");
    char lp[128];
    lc_copy(lp, sizeof(lp), p);
    return lp[0] && strcmp(lp, norm) == 0;
}

/* PoP: is_provider_explicitly_configured @ hermes_cli/auth.py:is_provider_explicitly_configured */
bool is_provider_explicitly_configured(const char *provider_id)
{
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    if (!norm[0]) return false;

    /* 1. auth.json active_provider */
    json_t *store = authstore_load(NULL);
    {
        char act[128];
        lc_copy(act, sizeof(act), json_get_str(store, "active_provider", ""));
        if (act[0] && strcmp(act, norm) == 0) { json_free(store); return true; }
    }
    json_free(store);

    /* 2. config.yaml model.provider + MoA slots */
    json_t *cfg = config_py_read_raw_config();
    if (cfg) {
        json_t *model = json_obj_get(cfg, "model");
        if (model && model->type == JSON_OBJECT) {
            char cp[128];
            lc_copy(cp, sizeof(cp), json_get_str(model, "provider", ""));
            if (cp[0] && strcmp(cp, norm) == 0) { json_free(cfg); return true; }
        }
        json_t *moa = json_obj_get(cfg, "moa");
        if (moa && moa->type == JSON_OBJECT) {
            json_t *refs = json_obj_get(moa, "reference_models");
            if (refs && refs->type == JSON_ARRAY)
                for (size_t i = 0; i < refs->c.count; i++)
                    if (moa_slot_matches(refs->c.items[i], norm)) { json_free(cfg); return true; }
            if (moa_slot_matches(json_obj_get(moa, "aggregator"), norm)) { json_free(cfg); return true; }
            json_t *presets = json_obj_get(moa, "presets");
            if (presets && presets->type == JSON_OBJECT) {
                for (size_t k = 0; k < presets->c.count; k++) {
                    json_t *preset = presets->c.items[k];
                    if (!preset || preset->type != JSON_OBJECT) continue;
                    json_t *prefs = json_obj_get(preset, "reference_models");
                    if (prefs && prefs->type == JSON_ARRAY)
                        for (size_t i = 0; i < prefs->c.count; i++)
                            if (moa_slot_matches(prefs->c.items[i], norm)) { json_free(cfg); return true; }
                    if (moa_slot_matches(json_obj_get(preset, "aggregator"), norm)) { json_free(cfg); return true; }
                }
            }
        }
        json_free(cfg);
    }

    /* 3. provider-specific env vars (excluding CLAUDE_CODE_OAUTH_TOKEN) */
    const catalog_provider_t *pc = provider_registry_get(norm);
    if (pc && pc->auth_type && strcmp(pc->auth_type, "api_key") == 0 && pc->api_key_env_vars) {
        for (int i = 0; pc->api_key_env_vars[i]; i++) {
            const char *var = pc->api_key_env_vars[i];
            if (strcmp(var, "CLAUDE_CODE_OAUTH_TOKEN") == 0) continue;
            const char *val = getenv(var);
            if (val && has_usable_secret(val, 4)) return true;
        }
    }

    /* 4. explicit-flow credential-pool entries (env-backed require live var) */
    json_t *entries = read_credential_pool(norm);
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < entries->c.count; i++) {
            json_t *e = entries->c.items[i];
            if (!e || e->type != JSON_OBJECT) continue;
            char src[256];
            lc_copy(src, sizeof(src), json_get_str(e, "source", ""));
            if (!src[0]) continue;
            if (strncmp(src, "env:", 4) == 0) {
                /* re-read the ORIGINAL (unlowered) source for the var name */
                const char *orig = json_get_str(e, "source", "");
                const char *var = strchr(orig, ':');
                if (var && *(var + 1)) {
                    char vn[128];
                    size_t n = strlen(var + 1);
                    while (n > 0 && isspace((unsigned char)var[n])) n--;
                    const char *s = var + 1;
                    while (*s && isspace((unsigned char)*s)) { s++; n--; }
                    if (n >= sizeof(vn)) n = sizeof(vn) - 1;
                    memcpy(vn, s, n); vn[n] = '\0';
                    const char *val = vn[0] ? getenv(vn) : NULL;
                    if (val && has_usable_secret(val, 4)) { json_free(entries); return true; }
                }
                continue;
            }
            if (strcmp(src, "device_code") == 0 || strcmp(src, "loopback_pkce") == 0 ||
                strcmp(src, "hermes_pkce") == 0 || strcmp(src, "manual") == 0 ||
                strncmp(src, "manual:", 7) == 0) {
                json_free(entries);
                return true;
            }
        }
    }
    if (entries) json_free(entries);
    return false;
}

/* PoP: clear_provider_auth @ hermes_cli/auth.py:clear_provider_auth */
bool clear_provider_auth(const char *provider_id)
{
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return false;
    json_t *store = authstore_load(NULL);
    const char *target = provider_id;
    if (!target || !*target) target = json_get_str(store, "active_provider", NULL);
    if (!target || !*target) { json_free(store); authstore_unlock(NULL); return false; }

    char tgt[128];
    snprintf(tgt, sizeof(tgt), "%s", target);

    json_t *providers = json_obj_get(store, "providers");
    if (!providers || providers->type != JSON_OBJECT) {
        json_set(store, "providers", json_new_object());
        providers = json_obj_get(store, "providers");
    }
    json_t *pool = json_obj_get(store, "credential_pool");
    if (!pool || pool->type != JSON_OBJECT) {
        json_set(store, "credential_pool", json_new_object());
        pool = json_obj_get(store, "credential_pool");
    }

    bool cleared = false;
    if (json_obj_del(providers, tgt)) cleared = true;
    if (json_obj_del(pool, tgt)) cleared = true;
    const char *active = json_get_str(store, "active_provider", NULL);
    if (active && strcmp(active, tgt) == 0) {
        json_set(store, "active_provider", json_null());
        cleared = true;
    }
    if (cleared) {
        char *saved = authstore_save(store, NULL);
        free(saved);
    }
    json_free(store);
    authstore_unlock(NULL);
    return cleared;
}

/* PoP: deactivate_provider @ hermes_cli/auth.py:deactivate_provider */
void deactivate_provider(void)
{
    if (!authstore_lock(NULL, AUTH_LOCK_TIMEOUT_SECONDS_C)) return;
    json_t *store = authstore_load(NULL);
    json_set(store, "active_provider", json_null());
    char *saved = authstore_save(store, NULL);
    free(saved);
    json_free(store);
    authstore_unlock(NULL);
}

/* PoP: get_anthropic_key @ hermes_cli/auth.py:get_anthropic_key */
char *get_anthropic_key(void)
{
    const catalog_provider_t *pc = provider_registry_get("anthropic");
    if (pc && pc->api_key_env_vars) {
        for (int i = 0; pc->api_key_env_vars[i]; i++) {
            char *val = config_py_get_env_value_prefer_dotenv(pc->api_key_env_vars[i]);
            if (val && *val) return val;
            free(val);
        }
    }
    return strdup("");
}

/* ════════════════════════════════════════════════════════════════
 *  config.yaml provider helpers
 * ════════════════════════════════════════════════════════════════ */

/* PoP: auth_get_config_provider @ hermes_cli/auth.py:_get_config_provider */
char *auth_get_config_provider(void)
{
    json_t *cfg = config_py_read_raw_config();
    if (!cfg) return NULL;
    json_t *model = json_obj_get(cfg, "model");
    char *out = NULL;
    if (model && model->type == JSON_OBJECT) {
        const char *p = json_get_str(model, "provider", NULL);
        if (p) {
            char norm[128];
            lc_copy(norm, sizeof(norm), p);
            if (norm[0]) out = strdup(norm);
        }
    }
    json_free(cfg);
    return out;
}

/* PoP: auth_config_provider_matches @ hermes_cli/auth.py:_config_provider_matches */
bool auth_config_provider_matches(const char *provider_id)
{
    if (!provider_id || !*provider_id) return false;
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    char *cur = auth_get_config_provider();
    bool match = cur && strcmp(cur, norm) == 0;
    free(cur);
    return match;
}

/* PoP: auth_should_reset_config_provider_on_logout @ hermes_cli/auth.py:_should_reset_config_provider_on_logout */
bool auth_should_reset_config_provider_on_logout(const char *provider_id)
{
    if (!provider_id || !*provider_id) return false;
    char norm[128];
    lc_copy(norm, sizeof(norm), provider_id);
    if (!provider_registry_get(norm)) return false;
    return auth_config_provider_matches(norm);
}

/* PoP: auth_logout_default_provider_from_config @ hermes_cli/auth.py:_logout_default_provider_from_config */
char *auth_logout_default_provider_from_config(void)
{
    char *provider = auth_get_config_provider();
    if (!provider) return NULL;
    if (strcmp(provider, "nous") == 0 || strcmp(provider, "openai-codex") == 0 ||
        strcmp(provider, "xai-oauth") == 0)
        return provider;
    free(provider);
    return NULL;
}

/* PoP: auth_reset_config_provider @ hermes_cli/auth.py:_reset_config_provider */
char *auth_reset_config_provider(void)
{
    char path[PATH_MAX];
    config_py_get_config_path(path, sizeof(path));
    struct stat st;
    if (stat(path, &st) != 0) return strdup(path);
    if (config_py_require_readable_config_before_write(path) != 0) return strdup(path);

    json_t *cfg = config_py_read_raw_config();
    if (!cfg || cfg->c.count == 0) { if (cfg) json_free(cfg); return strdup(path); }

    json_t *model = json_obj_get(cfg, "model");
    if (model && model->type == JSON_OBJECT) {
        json_set(model, "provider", json_new_string("auto"));
        if (json_obj_get(model, "base_url"))
            json_set(model, "base_url", json_new_string(OPENROUTER_BASE_URL_C));
        config_py_atomic_config_write(path, cfg);
    }
    json_free(cfg);
    return strdup(path);
}

/* PoP: auth_save_model_choice @ hermes_cli/auth.py:_save_model_choice */
void auth_save_model_choice(const char *model_id)
{
    if (!model_id || !*model_id) return;
    char path[PATH_MAX];
    config_py_get_config_path(path, sizeof(path));

    json_t *cfg = config_py_read_raw_config();
    if (!cfg) cfg = json_new_object();

    json_t *model = json_obj_get(cfg, "model");
    if (model && model->type == JSON_OBJECT) {
        json_set(model, "default", json_new_string(model_id));
    } else {
        json_t *m = json_new_object();
        json_set(m, "default", json_new_string(model_id));
        json_set(cfg, "model", m);
    }
    config_py_atomic_config_write(path, cfg);
    json_free(cfg);
}

/* ════════════════════════════════════════════════════════════════
 *  TLS verify resolution
 * ════════════════════════════════════════════════════════════════ */

static bool truthy_str(const char *v, bool def)
{
    if (!v) return def;
    char lo[16];
    lc_copy(lo, sizeof(lo), v);
    if (!lo[0]) return def;
    return strcmp(lo, "1") == 0 || strcmp(lo, "true") == 0 ||
           strcmp(lo, "yes") == 0 || strcmp(lo, "on") == 0;
}

/* PoP: auth_default_verify @ hermes_cli/auth.py:_default_verify */
auth_verify_t auth_default_verify(void)
{
    /* Linux/WSL: system trust store is fine — httpx's `True` default. The
     * darwin/certifi branch is a macOS-only workaround; on this platform the
     * faithful behavior is verification-on with default CAs. */
    auth_verify_t v; v.insecure = false; v.ca_bundle[0] = '\0';
    return v;
}

/* PoP: auth_resolve_verify @ hermes_cli/auth.py:_resolve_verify */
auth_verify_t auth_resolve_verify(const char *insecure_opt, const char *ca_bundle,
                                  const json_t *auth_state)
{
    const json_t *tls = NULL;
    if (auth_state && auth_state->type == JSON_OBJECT) {
        tls = json_obj_get(auth_state, "tls");
        if (tls && tls->type != JSON_OBJECT) tls = NULL;
    }

    bool effective_insecure;
    if (insecure_opt) {
        effective_insecure = truthy_str(insecure_opt, false);
    } else if (tls) {
        json_t *ins = json_obj_get(tls, "insecure");
        if (ins && ins->type == JSON_BOOL) effective_insecure = ins->bool_val;
        else effective_insecure = truthy_str(json_get_str(tls, "insecure", NULL), false);
    } else {
        effective_insecure = false;
    }

    const char *effective_ca = ca_bundle && *ca_bundle ? ca_bundle : NULL;
    if (!effective_ca && tls) {
        const char *c = json_get_str(tls, "ca_bundle", NULL);
        if (c && *c) effective_ca = c;
    }
    if (!effective_ca) {
        const char *c = getenv("HERMES_CA_BUNDLE");
        if (!c || !*c) c = getenv("SSL_CERT_FILE");
        if (!c || !*c) c = getenv("REQUESTS_CA_BUNDLE");
        if (c && *c) effective_ca = c;
    }

    auth_verify_t v; v.insecure = false; v.ca_bundle[0] = '\0';
    if (effective_insecure) { v.insecure = true; return v; }
    if (effective_ca) {
        struct stat st;
        if (stat(effective_ca, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(v.ca_bundle, sizeof(v.ca_bundle), "%s", effective_ca);
            return v;
        }
        hermes_log(LOG_WARNING, "auth",
                   "CA bundle path does not exist: %s — falling back to default certificates",
                   effective_ca);
        return auth_default_verify();
    }
    return auth_default_verify();
}

/* ════════════════════════════════════════════════════════════════
 *  OAuth trace + pure Nous helpers
 * ════════════════════════════════════════════════════════════════ */

#define DEFAULT_NOUS_PORTAL_URL_C "https://portal.nousresearch.com"

/* PoP: auth_oauth_trace_enabled @ hermes_cli/auth.py:_oauth_trace_enabled */
bool auth_oauth_trace_enabled(void)
{
    return truthy_str(getenv("HERMES_OAUTH_TRACE"), false);
}

/* PoP: auth_oauth_trace @ hermes_cli/auth.py:_oauth_trace */
void auth_oauth_trace(const char *event, const char *sequence_id, const json_t *fields)
{
    if (!auth_oauth_trace_enabled()) return;
    json_t *payload = json_new_object();
    json_set(payload, "event", json_new_string(event ? event : ""));
    if (sequence_id && *sequence_id)
        json_set(payload, "sequence_id", json_new_string(sequence_id));
    if (fields && fields->type == JSON_OBJECT)
        for (size_t i = 0; i < fields->c.count; i++)
            json_set(payload, fields->c.keys[i], json_copy(fields->c.items[i]));
    char *js = json_serialize(payload);
    if (js) {
        hermes_log(LOG_INFO, "auth", "oauth_trace %s", js);
        free(js);
    }
    json_free(payload);
}

/* _token_fingerprint: reuse auth_token_fingerprint() from auth_helpers.c */

/* _optional_base_url: reuse auth_optional_base_url() from auth_helpers.c */

/* PoP: auth_nous_inference_env_override @ hermes_cli/auth.py:_nous_inference_env_override */
char *auth_nous_inference_env_override(void)
{
    return auth_optional_base_url(getenv("NOUS_INFERENCE_BASE_URL"));
}

/* PoP: auth_nous_portal_env_override @ hermes_cli/auth.py:_nous_portal_env_override */
char *auth_nous_portal_env_override(void)
{
    const char *v = getenv("HERMES_PORTAL_BASE_URL");
    if (!v || !*v) v = getenv("NOUS_PORTAL_BASE_URL");
    return auth_optional_base_url(v);
}

/* extract hostname (lowercased) from a URL into buf; returns false on malformed */
static bool url_hostname(const char *url, char *buf, size_t sz, char *scheme, size_t ssz)
{
    if (scheme) scheme[0] = '\0';
    buf[0] = '\0';
    const char *p = strstr(url, "://");
    if (!p) return false;
    if (scheme) {
        size_t sl = (size_t)(p - url);
        if (sl >= ssz) sl = ssz - 1;
        for (size_t i = 0; i < sl; i++) scheme[i] = (char)tolower((unsigned char)url[i]);
        scheme[sl] = '\0';
    }
    p += 3;
    /* skip userinfo */
    const char *at = strchr(p, '@');
    const char *slash = strchr(p, '/');
    if (at && (!slash || at < slash)) p = at + 1;
    size_t i = 0;
    while (*p && *p != '/' && *p != ':' && *p != '?' && *p != '#' && i < sz - 1)
        buf[i++] = (char)tolower((unsigned char)*p++);
    buf[i] = '\0';
    return i > 0;
}

/* PoP: auth_validate_nous_inference_url_from_network @ hermes_cli/auth.py:_validate_nous_inference_url_from_network */
char *auth_validate_nous_inference_url_from_network(const char *url)
{
    if (!url) return NULL;
    while (*url && isspace((unsigned char)*url)) url++;
    size_t n = strlen(url);
    while (n > 0 && isspace((unsigned char)url[n-1])) n--;
    if (n == 0) return NULL;
    char cleaned[1024];
    if (n >= sizeof(cleaned)) return NULL;
    memcpy(cleaned, url, n); cleaned[n] = '\0';

    char host[256], scheme[16];
    if (!url_hostname(cleaned, host, sizeof(host), scheme, sizeof(scheme))) return NULL;
    if (strcmp(scheme, "https") != 0) {
        hermes_log(LOG_WARNING, "auth",
                   "nous: refusing non-https inference URL scheme '%s' from Portal response", scheme);
        return NULL;
    }
    if (strcmp(host, "inference-api.nousresearch.com") != 0) {
        hermes_log(LOG_WARNING, "auth",
                   "nous: refusing inference URL host '%s' from Portal response "
                   "(not in allowlist); falling back to default", host);
        return NULL;
    }
    while (n > 0 && cleaned[n-1] == '/') cleaned[--n] = '\0';
    return strdup(cleaned);
}

/* PoP: auth_migrate_stale_nous_portal_url @ hermes_cli/auth.py:_migrate_stale_nous_portal_url */
void auth_migrate_stale_nous_portal_url(json_t *providers)
{
    if (!providers || providers->type != JSON_OBJECT) return;
    json_t *nous = json_obj_get(providers, "nous");
    if (!nous || nous->type != JSON_OBJECT) return;
    const char *stored = json_get_str(nous, "portal_base_url", "");
    while (*stored && isspace((unsigned char)*stored)) stored++;
    if (!*stored) return;
    char host[256];
    if (url_hostname(stored, host, sizeof(host), NULL, 0) &&
        strcmp(host, "api.nousresearch.com") == 0) {
        hermes_log(LOG_WARNING, "auth",
                   "auth: migrating stale nous portal_base_url %s -> %s",
                   stored, DEFAULT_NOUS_PORTAL_URL_C);
        json_set(nous, "portal_base_url", json_new_string(DEFAULT_NOUS_PORTAL_URL_C));
    }
}

/* _parse_iso_timestamp: reuse auth_parse_iso_timestamp() from auth_helpers.c */

/* _is_expiring: reuse auth_is_expiring() from auth_helpers.c */

/* json_t-input TTL coercer; string form is auth_coerce_ttl_seconds() in auth_helpers.c */
static int auth_coerce_ttl_seconds_json(const json_t *expires_in)
{
    if (!expires_in) return 0;
    long ttl = 0;
    if (expires_in->type == JSON_NUMBER) ttl = (long)expires_in->num_val;
    else if (expires_in->type == JSON_STRING && expires_in->str_val)
        ttl = strtol(expires_in->str_val, NULL, 10);
    return ttl > 0 ? (int)ttl : 0;
}

/* PoP: auth_codex_access_token_is_expiring @ hermes_cli/auth.py:_codex_access_token_is_expiring */
bool auth_codex_access_token_is_expiring(const char *access_token, int skew_seconds)
{
    char *payload = auth_decode_jwt_payload(access_token);
    if (!payload) return false;
    double exp = auth_jwt_get_num(payload, "exp");
    free(payload);
    if (exp < 0) return false;
    int skew = skew_seconds > 0 ? skew_seconds : 0;
    return exp <= ((double)time(NULL) + skew);
}

/* PoP: auth_qwen_access_token_is_expiring @ hermes_cli/auth.py:_qwen_access_token_is_expiring */
bool auth_qwen_access_token_is_expiring(const char *expiry_date_ms, int skew_seconds)
{
    if (!expiry_date_ms || !*expiry_date_ms) return true;
    char *end = NULL;
    long long expiry_ms = strtoll(expiry_date_ms, &end, 10);
    if (end == expiry_date_ms || (end && *end && !isspace((unsigned char)*end))) return true;
    int skew = skew_seconds > 0 ? skew_seconds : 0;
    return ((double)time(NULL) + skew) * 1000.0 >= (double)expiry_ms;
}

/* PoP: auth_nous_jwt_expires_at @ hermes_cli/auth.py:_nous_jwt_expires_at */
char *auth_nous_jwt_expires_at(const char *token, const char *fallback_expires_at)
{
    char *payload = auth_decode_jwt_payload(token);
    if (payload) {
        double exp = auth_jwt_get_num(payload, "exp");
        free(payload);
        if (exp > 0) {
            time_t t = (time_t)exp;
            struct tm tmv;
            gmtime_r(&t, &tmv);
            char *buf = malloc(40);
            if (buf) {
                strftime(buf, 40, "%Y-%m-%dT%H:%M:%S+00:00", &tmv);
                return buf;
            }
        }
    }
    return fallback_expires_at ? strdup(fallback_expires_at) : NULL;
}

/* PoP: auth_nous_invoke_jwt_is_usable @ hermes_cli/auth.py:_nous_invoke_jwt_is_usable */
bool auth_nous_invoke_jwt_is_usable(const char *token, const char *scope,
                                    const char *expires_at, int min_ttl_seconds)
{
    char *reason = auth_nous_invoke_jwt_status(token, scope, expires_at, min_ttl_seconds);
    bool usable = (reason == NULL);
    free(reason);
    return usable;
}

/* PoP: auth_assert_nous_inference_jwt_usable @ hermes_cli/auth.py:_assert_nous_inference_jwt_usable */
auth_error_t *auth_assert_nous_inference_jwt_usable(const json_t *state, const char *access_token)
{
    const char *token = access_token;
    if (!token && state && state->type == JSON_OBJECT)
        token = json_get_str(state, "access_token", NULL);
    const char *scope = state ? json_get_str(state, "scope", NULL) : NULL;
    const char *expires_at = state ? json_get_str(state, "expires_at", NULL) : NULL;

    char *reason = auth_nous_invoke_jwt_status(token, scope, expires_at,
                                               AUTH_NOUS_INVOKE_JWT_MIN_TTL_SECONDS);
    if (!reason) return NULL;
    char msg[512];
    snprintf(msg, sizeof(msg),
             "Nous Portal access token is not a usable inference JWT (%s). "
             "Re-authenticate with: hermes auth add nous", reason);
    auth_error_t *err = auth_error_new(msg, "nous", reason, true);
    free(reason);
    return err;
}

/* PoP: auth_log_nous_invoke_jwt_selected @ hermes_cli/auth.py:_log_nous_invoke_jwt_selected */
void auth_log_nous_invoke_jwt_selected(const char *access_token, const char *sequence_id)
{
    hermes_log(LOG_INFO, "auth", "Nous inference auth: using NAS invoke JWT");
    char *fp = auth_token_fingerprint(access_token);
    json_t *fields = json_new_object();
    json_set(fields, "access_token_fp", fp ? json_new_string(fp) : json_null());
    free(fp);
    auth_oauth_trace("nous_invoke_jwt_selected", sequence_id, fields);
    json_free(fields);
}

/* PoP: auth_set_nous_agent_key_from_invoke_jwt @ hermes_cli/auth.py:_set_nous_agent_key_from_invoke_jwt */
void auth_set_nous_agent_key_from_invoke_jwt(json_t *state, const char *obtained_at)
{
    if (!state || state->type != JSON_OBJECT) return;
    const char *access_token = json_get_str(state, "access_token", NULL);
    if (!access_token) return;
    const char *p = access_token;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return;

    char now_iso[40];
    { time_t t = time(NULL); struct tm tmv; gmtime_r(&t, &tmv);
      strftime(now_iso, sizeof(now_iso), "%Y-%m-%dT%H:%M:%S+00:00", &tmv); }

    const char *existing_obtained_at = json_get_str(state, "agent_key_obtained_at", NULL);
    const char *agent_key = json_get_str(state, "agent_key", NULL);
    char effective_obtained_at[64];
    if (obtained_at && *obtained_at) {
        snprintf(effective_obtained_at, sizeof(effective_obtained_at), "%s", obtained_at);
    } else if (agent_key && strcmp(agent_key, access_token) == 0 &&
               existing_obtained_at && *existing_obtained_at) {
        bool nonblank = false;
        for (const char *q = existing_obtained_at; *q; q++)
            if (!isspace((unsigned char)*q)) { nonblank = true; break; }
        snprintf(effective_obtained_at, sizeof(effective_obtained_at), "%s",
                 nonblank ? existing_obtained_at : now_iso);
    } else {
        snprintf(effective_obtained_at, sizeof(effective_obtained_at), "%s", now_iso);
    }

    const char *fallback = json_get_str(state, "expires_at", NULL);
    char *expires_at = auth_nous_jwt_expires_at(access_token, fallback);
    double expires_epoch = expires_at ? auth_parse_iso_timestamp(expires_at) : -1;
    int expires_in;
    if (expires_epoch >= 0) {
        double d = expires_epoch - (double)time(NULL);
        expires_in = d > 0 ? (int)d : 0;
    } else {
        expires_in = auth_coerce_ttl_seconds_json(json_obj_get(state, "expires_in"));
    }

    if (expires_at) {
        json_set(state, "expires_at", json_new_string(expires_at));
        json_set(state, "expires_in", json_int(expires_in));
    }
    json_set(state, "agent_key", json_new_string(access_token));
    json_set(state, "agent_key_id", json_null());
    json_set(state, "agent_key_expires_at",
             expires_at ? json_new_string(expires_at) : json_null());
    json_set(state, "agent_key_expires_in", json_int(expires_in));
    json_set(state, "agent_key_reused", json_bool(false));
    json_set(state, "agent_key_obtained_at", json_new_string(effective_obtained_at));
    free(expires_at);
}

/* PoP: auth_select_nous_invoke_jwt @ hermes_cli/auth.py:_select_nous_invoke_jwt */
void auth_select_nous_invoke_jwt(json_t *state, const char *access_token,
                                 const char *sequence_id)
{
    if (!state || state->type != JSON_OBJECT) return;
    if (access_token) {
        const char *p = access_token;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p) json_set(state, "access_token", json_new_string(access_token));
    }
    auth_set_nous_agent_key_from_invoke_jwt(state, NULL);
    auth_log_nous_invoke_jwt_selected(json_get_str(state, "access_token", NULL), sequence_id);
}

/* PoP: auth_nous_effective_provider_state @ hermes_cli/auth.py:_nous_effective_provider_state */
json_t *auth_nous_effective_provider_state(const json_t *state)
{
    json_t *out = json_new_object();
    if (!state || state->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < state->c.count; i++) {
        const char *key = state->c.keys[i];
        if (strcmp(key, "expires_in") == 0 || strcmp(key, "agent_key_expires_in") == 0)
            continue;
        json_set(out, key, json_copy(state->c.items[i]));
    }
    return out;
}

/* PoP: auth_agent_key_is_usable @ hermes_cli/auth.py:_agent_key_is_usable */
bool auth_agent_key_is_usable(const json_t *state, int min_ttl_seconds)
{
    if (!state || state->type != JSON_OBJECT) return false;
    const char *key = json_get_str(state, "agent_key", NULL);
    if (!key) return false;
    const char *p = key;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return false;
    return auth_nous_invoke_jwt_is_usable(
        key,
        json_get_str(state, "scope", NULL),
        json_get_str(state, "agent_key_expires_at", NULL),
        min_ttl_seconds > 0 ? min_ttl_seconds : 0);
}

/* PoP: auth_empty_nous_auth_status @ hermes_cli/auth.py:_empty_nous_auth_status */
json_t *auth_empty_nous_auth_status(void)
{
    json_t *s = json_new_object();
    json_set(s, "logged_in", json_bool(false));
    json_set(s, "portal_base_url", json_null());
    json_set(s, "inference_base_url", json_null());
    json_set(s, "access_expires_at", json_null());
    json_set(s, "agent_key_expires_at", json_null());
    json_set(s, "has_refresh_token", json_bool(false));
    json_set(s, "inference_credential_present", json_bool(false));
    json_set(s, "credential_source", json_null());
    return s;
}

/* process-level nous-auth-status memo (mirrors _nous_auth_status_cache) */
static json_t *g_nous_auth_status_cache = NULL;
static pthread_mutex_t g_nous_status_mu = PTHREAD_MUTEX_INITIALIZER;

/* PoP: auth_invalidate_nous_auth_status_cache @ hermes_cli/auth.py:invalidate_nous_auth_status_cache */
void auth_invalidate_nous_auth_status_cache(void)
{
    pthread_mutex_lock(&g_nous_status_mu);
    if (g_nous_auth_status_cache) { json_free(g_nous_auth_status_cache); g_nous_auth_status_cache = NULL; }
    pthread_mutex_unlock(&g_nous_status_mu);
}

/* PoP: auth_is_terminal_xai_oauth_refresh_error @ hermes_cli/auth.py:_is_terminal_xai_oauth_refresh_error */
bool auth_is_terminal_xai_oauth_refresh_error(const char *provider, const char *code,
                                              bool relogin_required)
{
    if (!provider || !code || !relogin_required) return false;
    if (strcmp(provider, "xai-oauth") != 0) return false;
    return strcmp(code, "xai_refresh_failed") == 0 ||
           strcmp(code, "xai_auth_missing_refresh_token") == 0;
}

/* PoP: auth_is_terminal_codex_oauth_refresh_error @ hermes_cli/auth.py:_is_terminal_codex_oauth_refresh_error */
bool auth_is_terminal_codex_oauth_refresh_error(const char *provider, const char *code,
                                                bool relogin_required)
{
    if (!provider || !code || !relogin_required) return false;
    if (strcmp(provider, "openai-codex") != 0) return false;
    return strcmp(code, "codex_refresh_failed") == 0 ||
           strcmp(code, "codex_auth_missing_refresh_token") == 0 ||
           strcmp(code, "invalid_grant") == 0 ||
           strcmp(code, "invalid_token") == 0 ||
           strcmp(code, "refresh_token_reused") == 0;
}

/* ════════════════════════════════════════════════════════════════
 *  Shared cross-profile Nous OAuth store (Python flat-dict schema)
 * ════════════════════════════════════════════════════════════════ */

#define DEFAULT_NOUS_INFERENCE_URL_C "https://inference-api.nousresearch.com/v1"
#define DEFAULT_NOUS_CLIENT_ID_C     "hermes-cli"
#define DEFAULT_NOUS_SCOPE_C         "inference:invoke"
#define NOUS_SHARED_STORE_FILENAME_C "nous_auth.json"

extern void hermes_get_home(char *buf, size_t sz);   /* hermes_core_types.h */

/* PoP: auth_nous_shared_auth_dir @ hermes_cli/auth.py:_nous_shared_auth_dir */
static char *auth_nous_shared_auth_dir(void)
{
    const char *override = getenv("HERMES_SHARED_AUTH_DIR");
    if (override) {
        while (*override && isspace((unsigned char)*override)) override++;
        if (*override) {
            if (override[0] == '~' && (override[1] == '/' || override[1] == '\0')) {
                const char *home = getenv("HOME");
                if (home) {
                    char *out = malloc(strlen(home) + strlen(override) + 2);
                    if (!out) return NULL;
                    sprintf(out, "%s%s", home, override + 1);
                    return out;
                }
            }
            return strdup(override);
        }
    }
    char root[1024];
    hermes_get_home(root, sizeof(root));
    if (!root[0]) return NULL;
    char *out = malloc(strlen(root) + 16);
    if (out) sprintf(out, "%s/shared", root);
    return out;
}

/* PoP: auth_nous_shared_store_path_py @ hermes_cli/auth.py:_nous_shared_store_path */
char *auth_nous_shared_store_path_py(void)
{
    char *dir = auth_nous_shared_auth_dir();
    if (!dir) return NULL;
    char *out = malloc(strlen(dir) + sizeof(NOUS_SHARED_STORE_FILENAME_C) + 2);
    if (out) sprintf(out, "%s/%s", dir, NOUS_SHARED_STORE_FILENAME_C);
    free(dir);
    return out;
}

/* PoP: auth_nous_shared_store_lock_py @ hermes_cli/auth.py:_nous_shared_store_lock */
static int auth_nous_shared_store_lock_py(double timeout_seconds, char **out_lock_path)
{
    *out_lock_path = NULL;
    char *path = auth_nous_shared_store_path_py();
    if (!path) return 1;   /* no home yet: proceed without locking */
    char *lp = lock_path_for(path);
    free(path);
    if (!lp) return 1;
    int ok = auth_file_lock_acquire(lp, timeout_seconds > 0 ? timeout_seconds
                                                            : AUTH_LOCK_TIMEOUT_SECONDS_C);
    if (ok) { *out_lock_path = lp; return 1; }
    free(lp);
    return 0;
}

static void auth_nous_shared_store_unlock_py(char *lock_path)
{
    if (!lock_path) return;
    auth_file_lock_release(lock_path);
    free(lock_path);
}

/* PoP: auth_read_shared_nous_state @ hermes_cli/auth.py:_read_shared_nous_state */
json_t *auth_read_shared_nous_state(void)
{
    char *path = auth_nous_shared_store_path_py();
    if (!path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) { free(path); return NULL; }
    char *err = NULL;
    json_t *payload = json_parse_file(path, &err);
    if (!payload) {
        hermes_log(LOG_DEBUG, "auth", "Shared Nous auth store at %s is unreadable: %s",
                   path, err ? err : "parse error");
        free(err); free(path);
        return NULL;
    }
    free(err); free(path);
    if (payload->type != JSON_OBJECT) { json_free(payload); return NULL; }
    const char *rt = json_get_str(payload, "refresh_token", NULL);
    const char *at = json_get_str(payload, "access_token", NULL);
    bool rt_ok = false, at_ok = false;
    for (const char *p = rt; p && *p; p++) if (!isspace((unsigned char)*p)) { rt_ok = true; break; }
    for (const char *p = at; p && *p; p++) if (!isspace((unsigned char)*p)) { at_ok = true; break; }
    if (!rt_ok || !at_ok) { json_free(payload); return NULL; }
    return payload;
}

/* PoP: auth_write_shared_nous_state @ hermes_cli/auth.py:_write_shared_nous_state */
void auth_write_shared_nous_state(const json_t *state)
{
    if (!state || state->type != JSON_OBJECT) return;
    const char *refresh_token = json_get_str(state, "refresh_token", NULL);
    const char *access_token = json_get_str(state, "access_token", NULL);
    bool rt_ok = false, at_ok = false;
    for (const char *p = refresh_token; p && *p; p++)
        if (!isspace((unsigned char)*p)) { rt_ok = true; break; }
    for (const char *p = access_token; p && *p; p++)
        if (!isspace((unsigned char)*p)) { at_ok = true; break; }
    if (!rt_ok || !at_ok) return;

    json_t *shared = json_new_object();
    json_set(shared, "_schema", json_int(1));
    json_set(shared, "access_token", json_new_string(access_token));
    json_set(shared, "refresh_token", json_new_string(refresh_token));
    const char *tt = json_get_str(state, "token_type", NULL);
    json_set(shared, "token_type", json_new_string(tt && *tt ? tt : "Bearer"));
    const char *sc = json_get_str(state, "scope", NULL);
    json_set(shared, "scope", json_new_string(sc && *sc ? sc : DEFAULT_NOUS_SCOPE_C));
    const char *ci = json_get_str(state, "client_id", NULL);
    json_set(shared, "client_id", json_new_string(ci && *ci ? ci : DEFAULT_NOUS_CLIENT_ID_C));
    const char *pb = json_get_str(state, "portal_base_url", NULL);
    json_set(shared, "portal_base_url",
             json_new_string(pb && *pb ? pb : DEFAULT_NOUS_PORTAL_URL_C));
    const char *ib = json_get_str(state, "inference_base_url", NULL);
    json_set(shared, "inference_base_url",
             json_new_string(ib && *ib ? ib : DEFAULT_NOUS_INFERENCE_URL_C));
    json_t *oa = json_obj_get(state, "obtained_at");
    json_set(shared, "obtained_at", oa ? json_copy(oa) : json_null());
    json_t *ea = json_obj_get(state, "expires_at");
    json_set(shared, "expires_at", ea ? json_copy(ea) : json_null());
    char *now = iso_now_utc();
    if (now) { json_set(shared, "updated_at", json_new_string(now)); free(now); }

    char *lock_path = NULL;
    if (!auth_nous_shared_store_lock_py(AUTH_LOCK_TIMEOUT_SECONDS_C, &lock_path)) {
        json_free(shared);
        return;
    }
    char *path = auth_nous_shared_store_path_py();
    if (path) {
        char dir[PATH_MAX];
        snprintf(dir, sizeof(dir), "%s", path);
        char *slash = strrchr(dir, '/');
        if (slash) { *slash = '\0'; if (dir[0]) mkdir(dir, 0700); }
        secure_parent_dir_c(path);

        char *payload = json_serialize_pretty(shared, 2);
        if (payload) {
            char tmp[PATH_MAX + 64];
            snprintf(tmp, sizeof(tmp), "%s.tmp.%d.%lx", path, (int)getpid(),
                     (unsigned long)time(NULL) ^ (unsigned long)(size_t)payload);
            int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
            if (fd >= 0) {
                size_t len = strlen(payload);
                ssize_t wr = write(fd, payload, len);
                fsync(fd);
                close(fd);
                if (wr == (ssize_t)len && rename(tmp, path) == 0) {
                    json_t *fields = json_new_object();
                    json_set(fields, "path", json_new_string(path));
                    char *fp = auth_token_fingerprint(refresh_token);
                    json_set(fields, "refresh_token_fp",
                             fp ? json_new_string(fp) : json_null());
                    free(fp);
                    auth_nous_shared_store_unlock_py(lock_path);
                    lock_path = NULL;
                    auth_oauth_trace("nous_shared_store_written", NULL, fields);
                    json_free(fields);
                } else {
                    unlink(tmp);
                    hermes_log(LOG_DEBUG, "auth", "Failed to write shared Nous auth store");
                }
            }
            free(payload);
        }
        free(path);
    }
    auth_nous_shared_store_unlock_py(lock_path);
    json_free(shared);
}

/* PoP: auth_clear_shared_nous_state @ hermes_cli/auth.py:_clear_shared_nous_state */
void auth_clear_shared_nous_state(const char *reason)
{
    char *lock_path = NULL;
    if (!auth_nous_shared_store_lock_py(AUTH_LOCK_TIMEOUT_SECONDS_C, &lock_path)) return;
    char *path = auth_nous_shared_store_path_py();
    if (path) { unlink(path); free(path); }
    auth_nous_shared_store_unlock_py(lock_path);
    json_t *fields = json_new_object();
    json_set(fields, "reason", json_new_string(reason ? reason : ""));
    auth_oauth_trace("nous_shared_store_cleared", NULL, fields);
    json_free(fields);
}

/* PoP: auth_merge_shared_nous_oauth_state @ hermes_cli/auth.py:_merge_shared_nous_oauth_state */
bool auth_merge_shared_nous_oauth_state(json_t *state)
{
    if (!state || state->type != JSON_OBJECT) return false;
    json_t *shared = auth_read_shared_nous_state();
    if (!shared) return false;

    const char *shared_refresh = json_get_str(shared, "refresh_token", "");
    /* strip for comparison */
    char srt[2048];
    lc_copy(srt, sizeof(srt), shared_refresh);   /* NOTE: lowercases too — see below */
    /* refresh tokens are case-sensitive: do a plain strip instead */
    {
        const char *s = shared_refresh;
        while (*s && isspace((unsigned char)*s)) s++;
        size_t n = strlen(s);
        while (n > 0 && isspace((unsigned char)s[n-1])) n--;
        if (n >= sizeof(srt)) n = sizeof(srt) - 1;
        memcpy(srt, s, n); srt[n] = '\0';
    }
    if (!srt[0]) { json_free(shared); return false; }

    const char *local_refresh = json_get_str(state, "refresh_token", "");
    char lrt[2048];
    {
        const char *s = local_refresh;
        while (*s && isspace((unsigned char)*s)) s++;
        size_t n = strlen(s);
        while (n > 0 && isspace((unsigned char)s[n-1])) n--;
        if (n >= sizeof(lrt)) n = sizeof(lrt) - 1;
        memcpy(lrt, s, n); lrt[n] = '\0';
    }

    double shared_exp = auth_parse_iso_timestamp(json_get_str(shared, "expires_at", NULL));
    double local_exp = auth_parse_iso_timestamp(json_get_str(state, "expires_at", NULL));
    if (shared_exp < 0) shared_exp = 0;
    if (local_exp < 0) local_exp = 0;

    bool refresh_changed = strcmp(srt, lrt) != 0;
    bool fresher_access = shared_exp > local_exp;
    if (!refresh_changed && !fresher_access) { json_free(shared); return false; }

    static const char *MERGE_KEYS[] = {
        "access_token", "refresh_token", "token_type", "scope", "client_id",
        "portal_base_url", "inference_base_url", "obtained_at", "expires_at",
    };
    for (size_t i = 0; i < sizeof(MERGE_KEYS)/sizeof(MERGE_KEYS[0]); i++) {
        json_t *v = json_obj_get(shared, MERGE_KEYS[i]);
        if (!v || v->type == JSON_NULL) continue;
        if (v->type == JSON_STRING && (!v->str_val || !v->str_val[0])) continue;
        json_set(state, MERGE_KEYS[i], json_copy(v));
    }
    json_free(shared);
    return true;
}

/* ════════════════════════════════════════════════════════════════
 *  Quarantine — remove dead OAuth material after terminal failures
 * ════════════════════════════════════════════════════════════════ */

/* _is_terminal_nous_refresh_error: reuse auth_is_terminal_nous_refresh_error()
 * from port_auth_helpers.c (int signature). */

/* PoP: auth_quarantine_nous_oauth_state @ hermes_cli/auth.py:_quarantine_nous_oauth_state */
void auth_quarantine_nous_oauth_state(json_t *state, const auth_error_t *error,
                                      const char *reason)
{
    if (!state || state->type != JSON_OBJECT) return;

    /* forensic log BEFORE clearing token material (must be WARNING) */
    json_t *forensic = json_new_object();
    json_set(forensic, "reason", json_new_string(reason ? reason : ""));
    json_set(forensic, "error_code",
             (error && error->code) ? json_new_string(error->code) : json_null());
    json_t *ci = json_obj_get(state, "client_id");
    json_set(forensic, "client_id", ci ? json_copy(ci) : json_null());
    json_t *aki = json_obj_get(state, "agent_key_id");
    json_set(forensic, "agent_key_id", aki ? json_copy(aki) : json_null());
    char *fp = auth_token_fingerprint(json_get_str(state, "refresh_token", NULL));
    json_set(forensic, "refresh_token_fp", fp ? json_new_string(fp) : json_null());
    free(fp);

    char *auth_path = auth_file_path();
    if (auth_path) {
        json_set(forensic, "auth_json_path", json_new_string(auth_path));
        struct stat st;
        if (stat(auth_path, &st) == 0) {
            json_set(forensic, "auth_json_size", json_int((long long)st.st_size));
            json_set(forensic, "auth_json_mtime", json_number((double)st.st_mtime));
            json_set(forensic, "auth_json_exists", json_bool(true));
        } else {
            json_set(forensic, "auth_json_exists", json_bool(false));
        }
        free(auth_path);
    }

    const char *expires_at_raw = json_get_str(state, "expires_at", NULL);
    if (expires_at_raw && *expires_at_raw) {
        double exp = auth_parse_iso_timestamp(expires_at_raw);
        if (exp >= 0)
            json_set(forensic, "token_already_expired",
                     json_bool(exp < (double)time(NULL)));
        else
            json_set(forensic, "token_already_expired", json_null());
    } else {
        json_set(forensic, "token_already_expired", json_null());
    }

    char *fj = json_serialize(forensic);
    hermes_log(LOG_WARNING, "auth",
               "Nous OAuth state quarantined (terminal auth death): %s",
               fj ? fj : "{}");
    free(fj);
    json_free(forensic);

    /* strip dead OAuth material in place (Python: state.pop(key, None)) */
    static const char *DEAD_KEYS[] = {
        "access_token", "refresh_token", "expires_at", "expires_in",
        "obtained_at", "agent_key", "agent_key_id", "agent_key_expires_at",
        "agent_key_expires_in", "agent_key_reused", "agent_key_obtained_at",
    };
    for (size_t k = 0; k < sizeof(DEAD_KEYS)/sizeof(DEAD_KEYS[0]); k++)
        json_obj_del(state, DEAD_KEYS[k]);

    json_t *last_err = json_new_object();
    json_set(last_err, "provider", json_new_string("nous"));
    json_set(last_err, "code",
             (error && error->code) ? json_new_string(error->code) : json_null());
    json_set(last_err, "message",
             (error && error->message) ? json_new_string(error->message) : json_new_string(""));
    json_set(last_err, "reason", json_new_string(reason ? reason : ""));
    json_set(last_err, "relogin_required", json_bool(true));
    char *now = iso_now_utc();
    if (now) { json_set(last_err, "at", json_new_string(now)); free(now); }
    json_set(state, "last_auth_error", last_err);

    auth_clear_shared_nous_state(reason);
    auth_invalidate_nous_auth_status_cache();
}

/* PoP: auth_quarantine_nous_pool_entries @ hermes_cli/auth.py:_quarantine_nous_pool_entries */
bool auth_quarantine_nous_pool_entries(json_t *auth_store, const auth_error_t *error,
                                       const char *reason)
{
    if (!auth_store || auth_store->type != JSON_OBJECT) return false;
    json_t *pool = json_obj_get(auth_store, "credential_pool");
    if (!pool || pool->type != JSON_OBJECT) return false;
    json_t *entries = json_obj_get(pool, "nous");
    if (!entries || entries->type != JSON_ARRAY) return false;

    json_t *retained = json_new_array();
    bool removed = false;
    for (size_t i = 0; i < entries->c.count; i++) {
        json_t *entry = entries->c.items[i];
        if (entry && entry->type == JSON_OBJECT) {
            const char *src = json_get_str(entry, "source", NULL);
            if (src && (strcmp(src, NOUS_DEVICE_CODE_SOURCE_C) == 0 ||
                        strcmp(src, "manual:" NOUS_DEVICE_CODE_SOURCE_C) == 0)) {
                removed = true;
                continue;
            }
        }
        json_append(retained, entry ? json_copy(entry) : json_null());
    }

    if (removed) {
        json_set(pool, "nous", retained);
        json_t *fields = json_new_object();
        json_set(fields, "reason", json_new_string(reason ? reason : ""));
        json_set(fields, "error_code",
                 (error && error->code) ? json_new_string(error->code) : json_null());
        auth_oauth_trace("nous_pool_device_code_quarantined", NULL, fields);
        json_free(fields);
    } else {
        json_free(retained);
    }
    return removed;
}
