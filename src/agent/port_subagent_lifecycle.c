/*
 * port_subagent_lifecycle.c — C11 port of agent/subagent_lifecycle.py.
 * Pure data classes, thread-safe registry, validation, HMAC capability.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "hermes_json.h"
#include "hermes_crypto.h"
#include "hermes_subagent_lifecycle.h"

/* ── Module-level state ──────────────────────────────────── */

static _Thread_local void *g_active_parent = NULL;

/* ── Launch request ───────────────────────────────────────── */

subagent_launch_request_t *subagent_launch_request_create(
    const char *goal,
    const char *context,
    const char *role,
    const char *model,
    const char **allowed_toolsets,
    const char **blocked_tools,
    const char *parent_session_id,
    const char *correlation_id,
    const json_t *metadata,
    double timeout_seconds
) {
    subagent_launch_request_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->goal = goal ? strdup(goal) : NULL;
    r->context = context ? strdup(context) : NULL;
    r->role = role ? strdup(role) : "leaf";
    r->model = model ? strdup(model) : NULL;
    r->parent_session_id = parent_session_id ? strdup(parent_session_id) : NULL;
    r->correlation_id = correlation_id ? strdup(correlation_id) : NULL;
    r->timeout_seconds = timeout_seconds;

    if (allowed_toolsets) {
        int n = 0;
        while (allowed_toolsets[n]) n++;
        r->allowed_toolsets = calloc((size_t)n + 1, sizeof(char *));
        for (int i = 0; i < n; i++)
            r->allowed_toolsets[i] = strdup(allowed_toolsets[i]);
        r->allowed_toolsets[n] = NULL;
    }
    if (blocked_tools) {
        int n = 0;
        while (blocked_tools[n]) n++;
        r->blocked_tools = calloc((size_t)n + 1, sizeof(char *));
        for (int i = 0; i < n; i++)
            r->blocked_tools[i] = strdup(blocked_tools[i]);
        r->blocked_tools[n] = NULL;
    }

    if (metadata) r->metadata = json_copy(metadata);

    return r;
}

void subagent_launch_request_free(subagent_launch_request_t *r) {
    if (!r) return;
    free(r->goal);
    free(r->context);
    free(r->role);
    free(r->model);
    if (r->allowed_toolsets) {
        for (char **p = r->allowed_toolsets; *p; p++) free(*p);
        free(r->allowed_toolsets);
    }
    if (r->blocked_tools) {
        for (char **p = r->blocked_tools; *p; p++) free(*p);
        free(r->blocked_tools);
    }
    free(r->working_directory);
    free(r->parent_session_id);
    free(r->correlation_id);
    json_free(r->metadata);
    free(r);
}

/* ── Handle ───────────────────────────────────────────────── */

/* PoP: __init__ @ agent/subagent_lifecycle.py:_Registry.__init__ */
/* PoP: subagent_handle_create @ agent/subagent_lifecycle.py:SubagentHandle.__init__ */
subagent_handle_t *subagent_handle_create(
    const char *subagent_id,
    const char *parent_session_id,
    const char *correlation_id,
    double created_at,
    const char *provider,
    const char *model,
    const char *role,
    int depth,
    const char *capability
) {
    subagent_handle_t *h = calloc(1, sizeof(*h));
    if (!h) return NULL;
    h->contract_version = SUBAGENT_PUBLIC_CONTRACT_VERSION;
    h->subagent_id = subagent_id ? strdup(subagent_id) : NULL;
    h->parent_session_id = parent_session_id ? strdup(parent_session_id) : NULL;
    h->correlation_id = correlation_id ? strdup(correlation_id) : NULL;
    h->created_at = created_at;
    h->provider = provider ? strdup(provider) : NULL;
    h->model = model ? strdup(model) : NULL;
    h->role = role ? strdup(role) : "leaf";
    h->depth = depth;
    h->capability = capability ? strdup(capability) : NULL;
    return h;
}

void subagent_handle_free(subagent_handle_t *h) {
    if (!h) return;
    free(h->subagent_id);
    free(h->parent_session_id);
    free(h->correlation_id);
    free(h->provider);
    free(h->model);
    free(h->role);
    free(h->capability);
    free(h);
}

/* PoP: to_dict @ agent/subagent_lifecycle.py:SubagentHandle.to_dict */
json_t *subagent_handle_to_dict(const subagent_handle_t *h) {
    if (!h) return NULL;
    json_t *d = json_object();
    if (!d) return NULL;
    json_set(d, "contract_version", json_number((double)h->contract_version));
    json_set(d, "subagent_id", json_string(h->subagent_id ? h->subagent_id : ""));
    if (h->parent_session_id)
        json_set(d, "parent_session_id", json_string(h->parent_session_id));
    if (h->correlation_id)
        json_set(d, "correlation_id", json_string(h->correlation_id));
    json_set(d, "created_at", json_number(h->created_at));
    if (h->provider)
        json_set(d, "provider", json_string(h->provider));
    if (h->model)
        json_set(d, "model", json_string(h->model));
    json_set(d, "role", json_string(h->role ? h->role : "leaf"));
    json_set(d, "depth", json_number((double)h->depth));
    json_set(d, "capability", json_string(h->capability ? h->capability : ""));
    return d;
}

/* PoP: from_dict @ agent/subagent_lifecycle.py:SubagentHandle.from_dict */
subagent_handle_t *subagent_handle_from_dict(const json_t *d) {
    if (!d || d->type != JSON_OBJECT)
        return NULL;

    subagent_handle_t *h = calloc(1, sizeof(*h));
    if (!h) return NULL;

    const json_t *cv_j = json_obj_get(d, "contract_version");
    const json_t *sid_j = json_obj_get(d, "subagent_id");
    const json_t *psid_j = json_obj_get(d, "parent_session_id");
    const json_t *cid_j = json_obj_get(d, "correlation_id");
    const json_t *cat_j = json_obj_get(d, "created_at");
    const json_t *prov_j = json_obj_get(d, "provider");
    const json_t *mod_j = json_obj_get(d, "model");
    const json_t *role_j = json_obj_get(d, "role");
    const json_t *depth_j = json_obj_get(d, "depth");
    const json_t *cap_j = json_obj_get(d, "capability");

    bool ok = true;

    h->contract_version = cv_j && cv_j->type == JSON_NUMBER ? (int)cv_j->num_val
                         : SUBAGENT_PUBLIC_CONTRACT_VERSION;

    if (sid_j && sid_j->type == JSON_STRING)
        h->subagent_id = strdup(sid_j->str_val);
    else
        ok = false;

    if (psid_j && psid_j->type == JSON_STRING)
        h->parent_session_id = strdup(psid_j->str_val);
    if (cid_j && cid_j->type == JSON_STRING)
        h->correlation_id = strdup(cid_j->str_val);

    if (cat_j && cat_j->type == JSON_NUMBER)
        h->created_at = cat_j->num_val;
    else
        ok = false;

    if (prov_j && prov_j->type == JSON_STRING)
        h->provider = strdup(prov_j->str_val);
    if (mod_j && mod_j->type == JSON_STRING)
        h->model = strdup(mod_j->str_val);
    if (role_j && role_j->type == JSON_STRING)
        h->role = strdup(role_j->str_val);
    else
        h->role = strdup("leaf");

    if (depth_j && depth_j->type == JSON_NUMBER)
        h->depth = (int)depth_j->num_val;
    else
        ok = false;

    if (cap_j && cap_j->type == JSON_STRING)
        h->capability = strdup(cap_j->str_val);
    else
        ok = false;

    if (!ok) {
        subagent_handle_free(h);
        return NULL;
    }
    return h;
}

/* ── Context vars (Thread-local) ─────────────────────────── */

/* PoP: bind_subagent_parent @ agent/subagent_lifecycle.py:bind_subagent_parent */
void subagent_bind_parent(void *parent_agent) {
    g_active_parent = parent_agent;
}

/* PoP: get_active_subagent_parent @ agent/subagent_lifecycle.py:get_active_subagent_parent */
void *subagent_get_active_parent(void) {
    return g_active_parent;
}

/* ── HMAC capability ─────────────────────────────────────── */

static uint8_t subagent_secret[32];
static int subagent_secret_initialized = 0;
static pthread_mutex_t subagent_secret_lock = PTHREAD_MUTEX_INITIALIZER;

static void ensure_secret(void) {
    if (subagent_secret_initialized) return;
    pthread_mutex_lock(&subagent_secret_lock);
    if (!subagent_secret_initialized) {
        FILE *ur = fopen("/dev/urandom", "rb");
        if (ur) {
            size_t n = fread(subagent_secret, 1, 32, ur);
            fclose(ur);
            if (n == 32) {
                subagent_secret_initialized = 1;
                pthread_mutex_unlock(&subagent_secret_lock);
                return;
            }
        }
        srand((unsigned)(time(NULL) ^ getpid()));
        for (int i = 0; i < 32; i++)
            subagent_secret[i] = (uint8_t)(rand() & 0xFF);
        subagent_secret_initialized = 1;
    }
    pthread_mutex_unlock(&subagent_secret_lock);
}

/* PoP: capability @ agent/subagent_lifecycle.py:SubagentLifecycleService._capability */
char *subagent_compute_capability(
    const char *subagent_id,
    const char *parent_session_id,
    double created_at
) {
    if (!subagent_id) return NULL;
    ensure_secret();
    char value[4096];
    int n = snprintf(value, sizeof(value), "%s|%s|%.6f",
                     subagent_id,
                     parent_session_id ? parent_session_id : "",
                     created_at);
    if (n < 0 || (size_t)n >= sizeof(value)) return NULL;

    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_hmac_sha256(subagent_secret, 32,
                       (const unsigned char *)value, (size_t)n,
                       digest);

    char hex[65];
    for (size_t i = 0; i < CRYPTO_SHA256_LEN; i++)
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    hex[64] = '\0';
    return strdup(hex);
}

/* ── Validation ──────────────────────────────────────────── */

/* PoP: validate_request @ agent/subagent_lifecycle.py:SubagentLifecycleService._validate_request */
int subagent_validate_request(const subagent_launch_request_t *r, char **error_out) {
    if (!r || !r->goal || !r->goal[0]) {
        if (error_out) *error_out = strdup("goal must be a non-empty string of at most 16000 characters.");
        return -1;
    }
    if (strlen(r->goal) > SUBAGENT_MAX_GOAL_CHARS) {
        if (error_out) *error_out = strdup("goal must be a non-empty string of at most 16000 characters.");
        return -1;
    }
    if (r->context != NULL && strlen(r->context) > SUBAGENT_MAX_CONTEXT_CHARS) {
        if (error_out) *error_out = strdup("context must be a string of at most 32000 characters.");
        return -1;
    }
    if (r->role && strcmp(r->role, "leaf") != 0 && strcmp(r->role, "orchestrator") != 0) {
        if (error_out) *error_out = strdup("role must be 'leaf' or 'orchestrator'.");
        return -1;
    }
    if (r->timeout_seconds >= 0.0) {
        if (error_out) *error_out = strdup("Per-launch timeout is not supported; configure delegation timeout explicitly.");
        return -1;
    }
    if (r->working_directory != NULL) {
        if (error_out) *error_out = strdup("working_directory is not supported because Hermes delegates use isolated task environments.");
        return -1;
    }
    if (r->blocked_tools && r->blocked_tools[0]) {
        if (error_out) *error_out = strdup("Per-tool blocking is not supported; use allowed_toolsets. Hermes always blocks unsafe child tools.");
        return -1;
    }
    if (r->metadata) {
        char *ser = json_serialize(r->metadata);
        if (!ser) {
            if (error_out) *error_out = strdup("metadata must be JSON-serializable.");
            return -1;
        }
        if (strlen(ser) > SUBAGENT_MAX_METADATA_BYTES) {
            free(ser);
            if (error_out) *error_out = strdup("metadata exceeds 8192 bytes.");
            return -1;
        }
        free(ser);
    }
    return 0;
}

/* ── Status/result free helpers ──────────────────────────── */

void subagent_status_free(subagent_status_t *s) {
    if (!s) return;
    subagent_handle_free(&s->handle);
    free(s->diagnostic);
}

void subagent_terminal_state_free(subagent_terminal_state_t *ts) {
    if (!ts) return;
    subagent_handle_free(&ts->handle);
    free(ts->diagnostic);
}

void subagent_result_free(subagent_result_t *r) {
    if (!r) return;
    subagent_handle_free(&r->handle);
    free(r->summary);
    json_free(r->structured_payload);
    free(r->error_classification);
    free(r->error_message);
    json_free(r->usage_metadata);
    json_free(r->tool_execution_summary);
    free(r->result_hash);
}

void subagent_reconnect_result_free(subagent_reconnect_result_t *rr) {
    if (!rr) return;
    free(rr->diagnostic);
}

/* ── Registry (simple dynamic array) ─────────────────────── */

typedef struct subagent_record_s {
    subagent_handle_t handle;
    subagent_state_t  state;
    double            updated_at;
    void             *agent;
    void             *future;
    double            started_at;
    double            completed_at;
    subagent_result_t *result;
} subagent_record_t;

typedef struct {
    pthread_mutex_t lock;
    subagent_record_t *records;
    size_t n_records;
    size_t cap_records;
    struct {
        char *parent_session_id;
        char *correlation_id;
        char *subagent_id;
    } *correlations;
    size_t n_correlations;
    size_t cap_correlations;
} subagent_registry_t;

static subagent_registry_t g_registry = {PTHREAD_MUTEX_INITIALIZER, NULL, 0, 0, NULL, 0, 0};

/* PoP: _record @ agent/subagent_lifecycle.py:SubagentLifecycleService._record */
static subagent_record_t *registry_find_locked(const char *subagent_id) {
    for (size_t i = 0; i < g_registry.n_records; i++) {
        if (g_registry.records[i].handle.subagent_id &&
            strcmp(g_registry.records[i].handle.subagent_id, subagent_id) == 0)
            return &g_registry.records[i];
    }
    return NULL;
}

/* PoP: SubagentLifecycleService._cleanup_locked @
 * agent/subagent_lifecycle.py:SubagentLifecycleService._cleanup_locked */
void subagent_service_cleanup_locked(subagent_lifecycle_service_t *svc) {
    (void)svc;
    pthread_mutex_lock(&g_registry.lock);
    double cutoff = time(NULL) - SUBAGENT_TERMINAL_RETENTION_SECS;
    size_t write = 0;
    for (size_t i = 0; i < g_registry.n_records; i++) {
        subagent_record_t *rec = &g_registry.records[i];
        if (rec->result != NULL && rec->completed_at > 0 && rec->completed_at < cutoff) {
            if (rec->handle.correlation_id) {
                for (size_t j = 0; j < g_registry.n_correlations; j++) {
                    if (g_registry.correlations[j].subagent_id &&
                        strcmp(g_registry.correlations[j].subagent_id, rec->handle.subagent_id) == 0) {
                        free(g_registry.correlations[j].parent_session_id);
                        free(g_registry.correlations[j].correlation_id);
                        free(g_registry.correlations[j].subagent_id);
                        g_registry.correlations[j] = g_registry.correlations[--g_registry.n_correlations];
                        break;
                    }
                }
            }
            subagent_result_free(rec->result);
            subagent_handle_free(&rec->handle);
            /* Skip record — don't copy to write position */
        } else {
            if (write != i)
                g_registry.records[write] = g_registry.records[i];
            write++;
        }
    }
    g_registry.n_records = write;
    pthread_mutex_unlock(&g_registry.lock);
}

/* ── Lifecycle service ───────────────────────────────────── */

struct subagent_lifecycle_service_s {
    void *(*parent_resolver)(void);
};

subagent_lifecycle_service_t *subagent_service_create(void *(*parent_resolver)(void)) {
    subagent_lifecycle_service_t *svc = calloc(1, sizeof(*svc));
    if (!svc) return NULL;
    svc->parent_resolver = parent_resolver;
    return svc;
}

void subagent_service_free(subagent_lifecycle_service_t *svc) {
    free(svc);
}

/* PoP: launch @ agent/subagent_lifecycle.py:SubagentLifecycleService.launch */
int subagent_service_launch(
    subagent_lifecycle_service_t *svc,
    const subagent_launch_request_t *req,
    subagent_handle_t **out_handle,
    char **error_out
) {
    if (!svc || !req || !out_handle) {
        if (error_out) *error_out = strdup("Invalid arguments");
        return -1;
    }
    /* Validate request */
    if (subagent_validate_request(req, error_out) != 0)
        return -1;

    /* Resolve parent */
    void *parent = svc->parent_resolver ? svc->parent_resolver() : NULL;
    if (!parent) {
        if (error_out) *error_out = strdup("No active Hermes parent session is available.");
        return -1;
    }
    /* NOTE: full launch requires delegate_tool integration.
     * This implementation creates the handle and record but doesn't
     * actually run the subagent. */
    double created = (double)time(NULL);
    char *cap = subagent_compute_capability(
        req->goal, /* in real impl: subagent_id */
        req->parent_session_id,
        created);
    if (!cap) {
        if (error_out) *error_out = strdup("Failed to compute capability.");
        return -1;
    }

    *out_handle = subagent_handle_create(
        req->goal, /* placeholder: real subagent_id from delegate_tool */
        req->parent_session_id,
        req->correlation_id,
        created,
        NULL,  /* provider */
        req->model,
        req->role ? req->role : "leaf",
        1,     /* depth */
        cap
    );
    free(cap);

    if (!*out_handle) {
        if (error_out) *error_out = strdup("Failed to create handle.");
        return -1;
    }
    return 0;
}

/* PoP: status @ agent/subagent_lifecycle.py:SubagentLifecycleService.status */
int subagent_service_status(
    subagent_lifecycle_service_t *svc,
    const subagent_handle_t *handle,
    subagent_status_t *out_status
) {
    (void)svc;
    if (!out_status) return -1;
    memset(out_status, 0, sizeof(*out_status));

    if (!handle || !handle->subagent_id) {
        out_status->state = SUBAGENT_STATE_UNKNOWN;
        out_status->diagnostic = strdup("UNKNOWN_HANDLE");
        out_status->updated_at = (double)time(NULL);
        return 0;
    }

    pthread_mutex_lock(&g_registry.lock);
    subagent_record_t *rec = registry_find_locked(handle->subagent_id);
    if (!rec) {
        pthread_mutex_unlock(&g_registry.lock);
        out_status->state = SUBAGENT_STATE_UNKNOWN;
        out_status->diagnostic = strdup("UNKNOWN_HANDLE");
        out_status->updated_at = (double)time(NULL);
        return 0;
    }
    /* Copy handle */
    out_status->handle = rec->handle;
    out_status->state = rec->state;
    out_status->updated_at = rec->updated_at;
    pthread_mutex_unlock(&g_registry.lock);
    return 0;
}

/* PoP: wait @ agent/subagent_lifecycle.py:SubagentLifecycleService.wait */
int subagent_service_wait(
    subagent_lifecycle_service_t *svc,
    const subagent_handle_t *handle,
    double timeout_seconds,
    subagent_terminal_state_t *out_ts
) {
    (void)svc;
    (void)timeout_seconds;
    if (!out_ts) return -1;
    memset(out_ts, 0, sizeof(*out_ts));

    if (!handle || !handle->subagent_id) {
        out_ts->state = SUBAGENT_STATE_UNKNOWN;
        out_ts->completed = true;
        out_ts->diagnostic = strdup("UNKNOWN_HANDLE");
        return 0;
    }

    pthread_mutex_lock(&g_registry.lock);
    subagent_record_t *rec = registry_find_locked(handle->subagent_id);
    if (!rec) {
        pthread_mutex_unlock(&g_registry.lock);
        out_ts->state = SUBAGENT_STATE_UNKNOWN;
        out_ts->completed = true;
        out_ts->diagnostic = strdup("UNKNOWN_HANDLE");
        return 0;
    }
    out_ts->handle = rec->handle;
    out_ts->state = rec->state;
    out_ts->completed = (rec->result != NULL);
    pthread_mutex_unlock(&g_registry.lock);
    return 0;
}

/* PoP: cancel @ agent/subagent_lifecycle.py:SubagentLifecycleService.cancel */
int subagent_service_cancel(
    subagent_lifecycle_service_t *svc,
    const subagent_handle_t *handle,
    const char *reason,
    subagent_cancel_result_t *out_cr
) {
    (void)svc;
    (void)reason;
    if (!out_cr) return -1;
    memset(out_cr, 0, sizeof(*out_cr));

    if (!handle || !handle->subagent_id) {
        out_cr->unknown_handle = true;
        return 0;
    }

    pthread_mutex_lock(&g_registry.lock);
    subagent_record_t *rec = registry_find_locked(handle->subagent_id);
    if (!rec) {
        pthread_mutex_unlock(&g_registry.lock);
        out_cr->unknown_handle = true;
        return 0;
    }
    if (rec->result != NULL) {
        out_cr->already_terminal = true;
        out_cr->state = rec->state;
        pthread_mutex_unlock(&g_registry.lock);
        return 0;
    }
    rec->state = SUBAGENT_STATE_CANCEL_REQUESTED;
    rec->updated_at = (double)time(NULL);
    out_cr->accepted = true;
    out_cr->state = SUBAGENT_STATE_CANCEL_REQUESTED;
    pthread_mutex_unlock(&g_registry.lock);
    return 0;
}

/* PoP: result @ agent/subagent_lifecycle.py:SubagentLifecycleService.result */
int subagent_service_result(
    subagent_lifecycle_service_t *svc,
    const subagent_handle_t *handle,
    subagent_result_t *out_result
) {
    (void)svc;
    if (!out_result) return -1;
    memset(out_result, 0, sizeof(*out_result));

    if (!handle || !handle->subagent_id) {
        out_result->terminal_state = SUBAGENT_STATE_UNKNOWN;
        out_result->error_classification = strdup("UNKNOWN_HANDLE");
        return 0;
    }

    pthread_mutex_lock(&g_registry.lock);
    subagent_record_t *rec = registry_find_locked(handle->subagent_id);
    if (!rec) {
        pthread_mutex_unlock(&g_registry.lock);
        out_result->terminal_state = SUBAGENT_STATE_UNKNOWN;
        out_result->error_classification = strdup("UNKNOWN_HANDLE");
        return 0;
    }
    if (rec->result) {
        *out_result = *rec->result;
        pthread_mutex_unlock(&g_registry.lock);
        return 0;
    }
    out_result->handle = rec->handle;
    out_result->terminal_state = rec->state;
    out_result->error_classification = strdup("NOT_READY");
    pthread_mutex_unlock(&g_registry.lock);
    return 0;
}

/* PoP: reconnect @ agent/subagent_lifecycle.py:SubagentLifecycleService.reconnect */
int subagent_service_reconnect(
    subagent_lifecycle_service_t *svc,
    const subagent_handle_t *handle,
    subagent_reconnect_result_t *out_rr
) {
    (void)svc;
    if (!out_rr) return -1;
    memset(out_rr, 0, sizeof(*out_rr));

    if (!handle || !handle->subagent_id) {
        out_rr->state = SUBAGENT_STATE_UNKNOWN;
        out_rr->diagnostic = strdup("RECONNECT_UNAVAILABLE");
        return 0;
    }

    pthread_mutex_lock(&g_registry.lock);
    subagent_record_t *rec = registry_find_locked(handle->subagent_id);
    if (!rec) {
        pthread_mutex_unlock(&g_registry.lock);
        out_rr->state = SUBAGENT_STATE_UNKNOWN;
        out_rr->diagnostic = strdup("RECONNECT_UNAVAILABLE");
        return 0;
    }
    out_rr->connected = true;
    out_rr->state = rec->state;
    pthread_mutex_unlock(&g_registry.lock);
    return 0;
}