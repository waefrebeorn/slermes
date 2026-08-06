#ifndef HERMES_SUBAGENT_LIFECYCLE_H
#define HERMES_SUBAGENT_LIFECYCLE_H

/*
 * hermes_subagent_lifecycle.h — Public subagent lifecycle API.
 * Port of agent/subagent_lifecycle.py (pure data-classes + thread-safe registry).
 * C11, no C++.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ────────────────────────────────────────────── */

#define SUBAGENT_PUBLIC_CONTRACT_VERSION   1
#define SUBAGENT_MAX_GOAL_CHARS            16000
#define SUBAGENT_MAX_CONTEXT_CHARS         32000
#define SUBAGENT_MAX_METADATA_BYTES        8192
#define SUBAGENT_MAX_RESULT_CHARS          32000
#define SUBAGENT_TERMINAL_RETENTION_SECS   3600

/* ── Enums ────────────────────────────────────────────────── */

typedef enum {
    SUBAGENT_STATE_PENDING,
    SUBAGENT_STATE_STARTING,
    SUBAGENT_STATE_RUNNING,
    SUBAGENT_STATE_SUCCEEDED,
    SUBAGENT_STATE_FAILED,
    SUBAGENT_STATE_INTERRUPTED,
    SUBAGENT_STATE_CANCEL_REQUESTED,
    SUBAGENT_STATE_CANCELLED,
    SUBAGENT_STATE_UNKNOWN,
} subagent_state_t;

static inline const char *subagent_state_name(subagent_state_t s) {
    switch (s) {
        case SUBAGENT_STATE_PENDING:         return "PENDING";
        case SUBAGENT_STATE_STARTING:        return "STARTING";
        case SUBAGENT_STATE_RUNNING:         return "RUNNING";
        case SUBAGENT_STATE_SUCCEEDED:       return "SUCCEEDED";
        case SUBAGENT_STATE_FAILED:          return "FAILED";
        case SUBAGENT_STATE_INTERRUPTED:     return "INTERRUPTED";
        case SUBAGENT_STATE_CANCEL_REQUESTED:return "CANCEL_REQUESTED";
        case SUBAGENT_STATE_CANCELLED:       return "CANCELLED";
        case SUBAGENT_STATE_UNKNOWN:         return "UNKNOWN";
    }
    return "UNKNOWN";
}

/* ── Data classes (opaque — use accessors) ────────────────── */

/* SubagentLaunchRequest — goal + optional constraints */
typedef struct subagent_launch_request_s {
    char *goal;                     /* required, non-empty */
    char *context;                  /* optional, max 32K */
    char *role;                     /* "leaf" or "orchestrator" */
    char *model;                    /* optional */
    char **allowed_toolsets;        /* NULL-terminated array, or NULL */
    char **blocked_tools;           /* NULL-terminated array */
    char *working_directory;        /* optional (rejected by validate) */
    char *parent_session_id;        /* optional */
    char *correlation_id;           /* optional */
    json_t *metadata;               /* optional, must be JSON-serializable */
    double timeout_seconds;         /* optional (rejected by validate), or < 0 */
} subagent_launch_request_t;

/* SubagentHandle — immutable handle returned by launch */
typedef struct subagent_handle_s {
    int     contract_version;
    char   *subagent_id;
    char   *parent_session_id;      /* optional */
    char   *correlation_id;         /* optional */
    double  created_at;             /* time_t / unix seconds */
    char   *provider;               /* optional */
    char   *model;                  /* optional */
    char   *role;
    int     depth;
    char   *capability;             /* HMAC-derived */
} subagent_handle_t;

/* SubagentStatus — live status */
typedef struct subagent_status_s {
    subagent_handle_t handle;
    subagent_state_t  state;
    double            updated_at;
    char             *diagnostic;   /* optional */
} subagent_status_t;

/* SubagentTerminalState — final outcome */
typedef struct subagent_terminal_state_s {
    subagent_handle_t handle;
    subagent_state_t  state;
    bool              completed;
    bool              timed_out;
    char             *diagnostic;
} subagent_terminal_state_t;

/* SubagentCancelResult */
typedef struct subagent_cancel_result_s {
    bool             accepted;
    bool             already_terminal;
    bool             unknown_handle;
    bool             unsupported;
    subagent_state_t state;
} subagent_cancel_result_t;

/* SubagentResult */
typedef struct subagent_result_s {
    subagent_handle_t handle;
    subagent_state_t  terminal_state;
    bool              ready;
    char             *summary;
    json_t           *structured_payload;
    double            started_at;
    double            completed_at;
    char             *error_classification;
    char             *error_message;
    json_t           *usage_metadata;
    json_t           *tool_execution_summary;
    char             *result_hash;
} subagent_result_t;

/* SubagentReconnectResult */
typedef struct subagent_reconnect_result_s {
    bool             connected;
    subagent_state_t state;
    char            *diagnostic;
} subagent_reconnect_result_t;

/* ── Accessors ────────────────────────────────────────────── */

/* SubagentHandle */
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
);
void subagent_handle_free(subagent_handle_t *h);

/* PoP: to_dict @ agent/subagent_lifecycle.py:SubagentHandle.to_dict */
json_t *subagent_handle_to_dict(const subagent_handle_t *h);

/* PoP: from_dict @ agent/subagent_lifecycle.py:SubagentHandle.from_dict */
subagent_handle_t *subagent_handle_from_dict(const json_t *d);

/* ── Launch request helpers ───────────────────────────────── */
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
);
void subagent_launch_request_free(subagent_launch_request_t *r);

/* ── Context var helpers (module-level Thread-local) ──────── */

/* PoP: bind_subagent_parent @ agent/subagent_lifecycle.py:bind_subagent_parent */
void subagent_bind_parent(void *parent_agent);
/* PoP: get_active_subagent_parent @ agent/subagent_lifecycle.py:get_active_subagent_parent */
void *subagent_get_active_parent(void);

/* ── Validation ───────────────────────────────────────────── */

/* PoP: SubagentLifecycleService._validate_request @
 * agent/subagent_lifecycle.py:SubagentLifecycleService._validate_request */
int subagent_validate_request(const subagent_launch_request_t *r, char **error_out);

/* ── Capability HMAC ──────────────────────────────────────── */

/* PoP: SubagentLifecycleService._capability @
 * agent/subagent_lifecycle.py:SubagentLifecycleService._capability */
char *subagent_compute_capability(
    const char *subagent_id,
    const char *parent_session_id,
    double created_at
);

/* ── Lifecycle service API ────────────────────────────────── */

typedef struct subagent_lifecycle_service_s subagent_lifecycle_service_t;

/* PoP: SubagentLifecycleService.__init__ @
 * agent/subagent_lifecycle.py:SubagentLifecycleService.__init__ */
subagent_lifecycle_service_t *subagent_service_create(
    void *(*parent_resolver)(void)
);

void subagent_service_free(subagent_lifecycle_service_t *svc);

/* PoP: launch @ agent/subagent_lifecycle.py:SubagentLifecycleService.launch */
int subagent_service_launch(
    subagent_lifecycle_service_t *svc,
    const subagent_launch_request_t *req,
    subagent_handle_t **out_handle,
    char **error_out
);

/* PoP: status @ agent/subagent_lifecycle.py:SubagentLifecycleService.status */
int subagent_service_status(
    subagent_lifecycle_service_t *svc,
    const subagent_handle_t *handle,
    subagent_status_t *out_status
);

/* PoP: wait @ agent/subagent_lifecycle.py:SubagentLifecycleService.wait */
int subagent_service_wait(
    subagent_lifecycle_service_t *svc,
    const subagent_handle_t *handle,
    double timeout_seconds,
    subagent_terminal_state_t *out_ts
);

/* PoP: cancel @ agent/subagent_lifecycle.py:SubagentLifecycleService.cancel */
int subagent_service_cancel(
    subagent_lifecycle_service_t *svc,
    const subagent_handle_t *handle,
    const char *reason,
    subagent_cancel_result_t *out_cr
);

/* PoP: result @ agent/subagent_lifecycle.py:SubagentLifecycleService.result */
int subagent_service_result(
    subagent_lifecycle_service_t *svc,
    const subagent_handle_t *handle,
    subagent_result_t *out_result
);

/* PoP: reconnect @ agent/subagent_lifecycle.py:SubagentLifecycleService.reconnect */
int subagent_service_reconnect(
    subagent_lifecycle_service_t *svc,
    const subagent_handle_t *handle,
    subagent_reconnect_result_t *out_rr
);

/* PoP: _cleanup_locked @ agent/subagent_lifecycle.py:SubagentLifecycleService._cleanup_locked */
void subagent_service_cleanup_locked(subagent_lifecycle_service_t *svc);

/* ── Module-level helpers ─────────────────────────────────── */
typedef struct json_t json_t;

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SUBAGENT_LIFECYCLE_H */