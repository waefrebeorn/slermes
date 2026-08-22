/*
 * approval_transport.h — Port of hermes_cli/approval_transport.py.
 *
 * Host-owned contract for plugin-provided human approval transports.
 * Transports only present an immutable, redacted request and return a
 * correlated human decision; they never participate in command detection or
 * authorization policy. The host validates scope, request binding and
 * timeout fail-closed (any failure is represented as a denial).
 *
 * Async callbacks (asyncio.run on a bounded daemon worker) have no C
 * analog — the C port runs sync present-functions on a bounded detached
 * worker thread with the same capacity bound (_MAX_ACTIVE_TRANSPORT_WORKERS
 * = 8), deadline, interruption and correlation checks. All decisions are
 * validated against the exact host-created request before being honored.
 */
#ifndef APPROVAL_TRANSPORT_H
#define APPROVAL_TRANSPORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APPROVAL_MAX_ACTIVE_TRANSPORT_WORKERS 8

/* PoP: ApprovalRequest @ hermes_cli/approval_transport.py (frozen dataclass) */
typedef struct {
    int schema_version;             /* always 1 */
    char *request_id;               /* uuid4().hex (owned) */
    char *digest;                   /* sha256 of canonical form (owned) */
    char *command;                  /* owned */
    char *description;              /* owned */
    char *pattern_key;              /* owned */
    char **pattern_keys;            /* owned array of owned strings */
    size_t n_pattern_keys;
    char *surface;                  /* owned */
    char *session_key;              /* owned — digest-only input, not displayed */
    double timeout_seconds;
    char **allowed_choices;         /* owned array of owned strings */
    size_t n_allowed_choices;
} approval_request_t;

/* PoP: ApprovalDecision @ hermes_cli/approval_transport.py */
typedef struct {
    char *request_id;       /* owned */
    char *request_digest;   /* owned */
    char *choice;           /* owned */
} approval_decision_t;

/* PoP: ApprovalTransportResult @ hermes_cli/approval_transport.py.
 * Any failure is represented as choice="deny" + failure set; success leaves
 * failure NULL. Buffers are fixed — failure codes are short literals. */
typedef struct {
    char choice[16];
    char failure[16]; /* "" == None */
} approval_transport_result_t;

/* PoP: uuid.uuid4().hex — real random uuid4, lowercase 32 hex chars.
 * malloc'd, caller frees. */
char *approval_uuid4_hex(void);

/* PoP: ApprovalRequest.create @ hermes_cli/approval_transport.py
 * Builds the immutable display-only request: allowed choices are
 * ["once"(,"session"(,"always")),"deny"]; digest is sha256 over the
 * sort-keys compact JSON of the canonical dict. Returns NULL on allocation
 * failure. Free with approval_request_free. */
approval_request_t *approval_request_create(
    const char *command, const char *description,
    const char *pattern_key, const char *const *pattern_keys,
    size_t n_pattern_keys, const char *session_key, const char *surface,
    int allow_session, int allow_permanent, double timeout_seconds);
void approval_request_free(approval_request_t *req);

/* PoP: ApprovalRequest.respond @ hermes_cli/approval_transport.py
 * Build the correlated response a transport should return.
 * malloc'd, caller frees with approval_decision_free. */
approval_decision_t *approval_request_respond(const approval_request_t *req,
                                              const char *choice);
void approval_decision_free(approval_decision_t *d);

/* PoP: invoke_approval_transport @ hermes_cli/approval_transport.py
 * Run a transport present-function on a bounded worker thread. Contract for
 * `present` (models the plugin callback's possible exits):
 *   return 0 and set *out  -> a decision value came back (validated below)
 *   return 1               -> callback returned a non-ApprovalDecision value
 *   return 2               -> callback raised
 * The worker releases its capacity slot when the callback returns; the host
 * waits up to timeout_seconds (fail-closed deny on expiry), honoring
 * is_interrupted between polls. Late results cannot authorize anything. */
typedef int (*approval_present_fn)(void *ctx, const approval_request_t *req,
                                   approval_decision_t **out);
typedef void (*approval_on_poll_fn)(void *ctx);
typedef int (*approval_is_interrupted_fn)(void *ctx);

approval_transport_result_t invoke_approval_transport(
    approval_present_fn present, void *present_ctx,
    const approval_request_t *request,
    double timeout_seconds, double poll_interval,
    approval_on_poll_fn on_poll, void *on_poll_ctx,
    approval_is_interrupted_fn is_interrupted, void *interrupted_ctx);

/* Recompute the canonical-form SHA-256 for an existing request (host
 * verification helper used by oracles to prove digest binding). Returns 1
 * when req->digest matches, 0 otherwise. */
int approval_request_verify_digest(const approval_request_t *req);

/* PoP: _transport_worker_slots — current active worker count (testing hook).
 * Exposed so hosts/oracles can observe the bounded-semaphore level. */
int approval_transport_active_workers(void);

#ifdef __cplusplus
}
#endif

#endif /* APPROVAL_TRANSPORT_H */
