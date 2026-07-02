/*
 * codex_app_server_session.h — Session adapter for codex app-server runtime.
 *
 * Owns one Codex thread per Hermes session. Drives turn/start, consumes
 * streaming notifications via CodexEventProjector, handles server-initiated
 * approval requests (apply_patch, exec command), translates cancellation,
 * and returns a clean turn result that the agent loop can splice into
 * its messages list.
 *
 * Maps to Python agent/transports/codex_app_server_session.py (846 lines).
 */

#ifndef CODEX_APP_SERVER_SESSION_H
#define CODEX_APP_SERVER_SESSION_H

#include "codex_app_server_client.h"
#include "codex_event_projector.h"
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward */
typedef struct codex_session_t codex_session_t;

/* Approval callback type:
 *   command     — the command/patch label shown to user
 *   description — human-readable description
 *   allow_permanent — whether "always/session" choices are offered
 * Returns: "once", "session", "always", or "deny" */
typedef const char *(*codex_approval_callback_t)(
    const char *command,
    const char *description,
    bool allow_permanent,
    void *user_data
);

/* Event callback type (display hook — kawaii spinner ticks etc.) */
typedef void (*codex_event_callback_t)(const char *notification_json, void *user_data);

/* Turn result */
typedef struct {
    char  *final_text;           /* Assistant text returned to caller (malloc'd) */
    json_node_t **projected_messages; /* Array of message objects */
    int    msg_count;
    int    msg_capacity;
    int    tool_iterations;      /* How many tool-shaped items completed */
    bool   interrupted;          /* True if interrupt fired mid-turn */
    char  *error;                /* Non-NULL if turn ended in error (malloc'd) */
    char  *turn_id;              /* Codex turn id (malloc'd) */
    char  *thread_id;            /* Codex thread id (malloc'd) */
    bool   should_retire;        /* True if subprocess is likely wedged */
} codex_turn_result_t;

/* Create a new session (does NOT spawn subprocess yet).
 * cwd: working directory for codex
 * codex_bin: path to codex binary (NULL = "codex")
 * codex_home: CODEX_HOME env var (NULL = unset)
 * permission_profile: codex permission profile id (NULL = workspace-write) */
codex_session_t *codex_session_new(
    const char *cwd,
    const char *codex_bin,
    const char *codex_home,
    const char *permission_profile
);

/* Free all resources */
void codex_session_free(codex_session_t *s);

/* Spawn subprocess, initialize handshake, thread/start.
 * Idempotent — repeated calls return the same thread id.
 * Returns thread_id (malloc'd, caller frees) or NULL on error. */
char *codex_session_ensure_started(codex_session_t *s);

/* Close subprocess and free resources */
void codex_session_close(codex_session_t *s);

/* Signal the active turn loop to issue turn/interrupt and unwind.
 * Idempotent. */
void codex_session_request_interrupt(codex_session_t *s);

/* Set approval callback */
void codex_session_set_approval_callback(
    codex_session_t *s,
    codex_approval_callback_t cb,
    void *user_data
);

/* Set event callback (display hook) */
void codex_session_set_event_callback(
    codex_session_t *s,
    codex_event_callback_t cb,
    void *user_data
);

/* Send a user message and block until turn/completed.
 * user_input: plain text string
 * turn_timeout: max seconds for the turn
 * notification_poll_timeout: seconds to wait for each notification poll
 * post_tool_quiet_timeout: seconds of silence after tool completion before fast-fail
 * Returns turn result (caller must free with codex_turn_result_free) */
codex_turn_result_t *codex_session_run_turn(
    codex_session_t *s,
    const char *user_input,
    double turn_timeout,
    double notification_poll_timeout,
    double post_tool_quiet_timeout
);

/* Free a turn result */
void codex_turn_result_free(codex_turn_result_t *r);

#ifdef __cplusplus
}
#endif

#endif /* CODEX_APP_SERVER_SESSION_H */
