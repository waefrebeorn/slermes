/*
 * browser_control_broker.h — C11 port of gateway/browser_control_broker.py
 *
 * Transport-neutral broker core: binds identity-scoped controllers to
 * callers. Registration tickets are single-use and expiring; selection is
 * exact-identity + capability; dispatch is one-pending-per-command-id with
 * scoped cancellation, deferred cancels for offline controllers, and
 * Developer Mode gating for privileged capabilities.
 */
#ifndef SLERMES_BROWSER_CONTROL_BROKER_H
#define SLERMES_BROWSER_CONTROL_BROKER_H

#include "hermes_json.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BCB_DEFAULT_TICKET_TTL 30.0
#define BCB_DEFAULT_COMMAND_TIMEOUT 30.0
#define BCB_MAX_DEFERRED_CANCELS 512
#define BCB_PROTOCOL_VERSION 1

/* Wire frame method names. */
#define BCB_FRAME_COMMAND "browser.controller.command"
#define BCB_FRAME_CANCEL "browser.controller.cancel"

/* Error codes (BrowserControlError subclasses). */
typedef enum {
    BCB_OK = 0,
    BCB_ERR_TICKET_INVALID = 1,
    BCB_ERR_UNAVAILABLE = 2,
    BCB_ERR_CANCELLED = 3,
    BCB_ERR_TIMEOUT = 4,
    BCB_ERR_REJECTED = 5
} bcb_error_t;

/* ControllerScope @ gateway/browser_control_broker.py:ControllerScope */
typedef struct {
    char principal_id[256];
    char profile_id[256];
    char session_id[256];
    char controller_id[256];
    char browser_profile_id[256];
    char transport_family[64];
    /* NULL-terminated array of malloc'd capability strings (owned). */
    char **capabilities;
    size_t capabilities_n;
} bcb_scope_t;

void bcb_scope_init(bcb_scope_t *scope);
void bcb_scope_free(bcb_scope_t *scope);
void bcb_scope_copy(bcb_scope_t *dst, const bcb_scope_t *src);

/* _scope_identity @ ...:_scope_identity — structural equality over all six
 * stable identity fields (capabilities excluded). */
int bcb_same_scope_identity(const bcb_scope_t *a, const bcb_scope_t *b);
/* Full dataclass equality including capabilities. */
int bcb_scope_equal(const bcb_scope_t *a, const bcb_scope_t *b);

/* Send callback: delivers a JSON command/cancel frame to the controller.
 * Invoked OUTSIDE the broker lock so the callback may synchronously
 * complete. ctx is the transport's own handle ("owner"). */
typedef void (*bcb_send_fn)(json_t *frame, void *ctx);

/* Artifact-store duck-type: validate(artifact_id, scope) → 0 when live. */
typedef int (*bcb_artifact_validate_fn)(void *store, const char *artifact_id,
                                        const bcb_scope_t *scope,
                                        char *err, size_t err_sz);
typedef struct {
    void *ctx;
    bcb_artifact_validate_fn validate;
} bcb_artifact_store_vtab_t;

typedef struct bcb_broker bcb_broker_t;

/* Capability allowlists. Each is a NULL-terminated string table; also
 * exposed as a membership probe. */
extern const char *const BCB_CAPABILITIES[];
extern const char *const BCB_DEVELOPER_CAPABILITIES[];
extern const char *const BCB_ARTIFACT_CAPABILITIES[];
int bcb_capability_in(const char *const *table, const char *cap);

/* browser_control_protocol_supported @ ...:browser_control_protocol_supported
 * Exact int match only (booleans rejected). */
int bcb_browser_control_protocol_supported(const json_t *value);

/* browser_control_developer_mode @ ...:browser_control_developer_mode
 * config may be NULL → read the live config.yaml flag. */
int bcb_browser_control_developer_mode(const json_t *config);

/* filter_browser_control_capabilities @ ...:filter_browser_control_capabilities
 * value: JSON array of capability strings. Returns a malloc'd NULL-
 * terminated array of malloc'd permitted caps; *out_n set. developer_mode:
 * -1 → consult live config. Caller frees each entry + the array. */
char **bcb_filter_browser_control_capabilities(const json_t *value,
                                               int developer_mode,
                                               size_t *out_n);

/* __init__ @ BrowserControlBroker.__init__ — clock_fn NULL → monotonic.
 * developer_mode: -1 → defer to live config on every selection; 0/1 pin. */
bcb_broker_t *bcb_broker_new(double ticket_ttl, double command_timeout,
                             double (*clock_fn)(void *), int developer_mode);
void bcb_broker_free(bcb_broker_t *broker);

/* attach_artifact_store / _artifact_store_for_scope */
void bcb_broker_attach_artifact_store(bcb_broker_t *broker,
                                      const bcb_artifact_store_vtab_t *store,
                                      const char *profile_id);
const bcb_artifact_store_vtab_t *bcb_broker_artifact_store_for_scope(
    bcb_broker_t *broker, const bcb_scope_t *scope);

/* developer_mode property */
int bcb_broker_developer_mode(bcb_broker_t *broker);

/* Ticket @ ...:Ticket */
typedef struct {
    char value[128];
    double expires_at;
} bcb_ticket_t;

/* mint_ticket @ ...:mint_ticket */
bcb_ticket_t *bcb_broker_mint_ticket(bcb_broker_t *broker,
                                     const bcb_scope_t *scope);
/* consume_ticket @ ...:consume_ticket — returns a copy of the scope on
 * success (caller frees capabilities via bcb_scope_free); else error code. */
bcb_error_t bcb_broker_consume_ticket(bcb_broker_t *broker, const char *value,
                                      bcb_scope_t *out_scope);
/* _prune_tickets @ ...:_prune_tickets */
void bcb_broker_prune_tickets(bcb_broker_t *broker, double now);

/* attach @ ...:attach — register or refresh the controller for one identity;
 * same-lane different identity hard-replaces; reconnect flushes deferred
 * cancel frames outside the lock (returns BCB_ERR_UNAVAILABLE on a failed
 * deferred-cancel flush). */
bcb_error_t bcb_broker_attach(bcb_broker_t *broker, const bcb_scope_t *scope,
                              bcb_send_fn send, void *owner);

/* select @ ...:select — returns 1 when exactly one connected controller
 * matches identity AND has negotiated ``capability``. */
int bcb_broker_select(bcb_broker_t *broker, const bcb_scope_t *scope,
                      const char *capability);

/* is_owner @ ...:is_owner */
int bcb_broker_is_owner(bcb_broker_t *broker, const bcb_scope_t *scope,
                        void *owner);

/* disconnect @ ...:disconnect — owner==NULL means "any owner" (the Python
 * _OWNER_UNSET sentinel maps to NULL). Returns 1 when marked offline. */
int bcb_broker_disconnect(bcb_broker_t *broker, const bcb_scope_t *scope,
                          void *owner);

/* detach @ ...:detach — remove controller, fail pending work closed. */
void bcb_broker_detach(bcb_broker_t *broker, const bcb_scope_t *scope,
                       void *owner, int notify_controller);

/* dispatch @ ...:dispatch — blocks up to command_timeout. On success fills
 * *result (caller frees) and returns BCB_OK; else the error code. */
bcb_error_t bcb_broker_dispatch(bcb_broker_t *broker, const bcb_scope_t *scope,
                                const char *action, const json_t *arguments,
                                const char *tool_call_id, json_t **result);

/* complete @ ...:complete — resolve by id; returns 0 when nothing pending. */
int bcb_broker_complete(bcb_broker_t *broker, const char *command_id,
                        const bcb_scope_t *scope, int ok, json_t *result);

/* cancel @ ...:cancel — cancel the scope+tool_call_id pending command. */
int bcb_broker_cancel(bcb_broker_t *broker, const bcb_scope_t *scope,
                      const char *tool_call_id);

/* scope_for_session @ ...:scope_for_session — unambiguous attached scope or
 * NULL. task_id participates only as a lookup-hint fallback. */
bcb_scope_t *bcb_broker_scope_for_session(bcb_broker_t *broker,
                                          const char *session_id,
                                          const char *task_id,
                                          const char *principal_id,
                                          const char *transport_family);

/* lane_registered @ ...:lane_registered */
int bcb_broker_lane_registered(bcb_broker_t *broker, const char *session_id,
                               const char *task_id, const char *principal_id,
                               const char *transport_family);

/* disconnect_owner / detach_owner @ ... */
int bcb_broker_disconnect_owner(bcb_broker_t *broker, void *owner);
int bcb_broker_detach_owner(bcb_broker_t *broker, void *owner,
                            int notify_controller);

/* reset @ ...:reset — fail all live work closed, clear tickets. */
void bcb_broker_reset(bcb_broker_t *broker);

double bcb_broker_ticket_ttl_seconds(bcb_broker_t *broker);
int bcb_broker_pending_count(bcb_broker_t *broker);

/* get_browser_control_broker @ ...:get_browser_control_broker */
bcb_broker_t *bcb_get_browser_control_broker(void);

/* browser_control_enabled @ ...:browser_control_enabled — config NULL →
 * live config.yaml read of browser.extension_control.enabled. */
int bcb_browser_control_enabled(const json_t *config);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_BROWSER_CONTROL_BROKER_H */
