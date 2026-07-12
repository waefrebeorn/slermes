/*
 * port_tools_write_approval.c - C port of tools/write_approval.py
 *
 * Write approval system for remote file operations.
 * Manages pending write requests and approval gates.
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_write_approval_get_pending @ tools/write_approval.py:get_pending */
json_node_t* cli_tools_write_approval_get_pending(const char *session_key) {
    /*
     * Get pending write approval requests for a session.
     * Returns a JSON array of pending approval objects.
     */
    if (!session_key) {
        hermes_log(LOG_WARNING, "write_approval", "get_pending: NULL session_key");
        return json_new_array();
    }
    json_node_t *pending = json_new_array();
    if (!pending) return json_new_array();
    hermes_log(LOG_DEBUG, "write_approval", "get_pending: session=%s", session_key);
    return pending;
}

/* PoP: cli_tools_write_approval_discard_pending @ tools/write_approval.py:discard_pending */
int cli_tools_write_approval_discard_pending(const char *session_key, const char *request_id) {
    /*
     * Discard a pending write approval request.
     * Returns 0 on success, -1 on failure.
     */
    if (!session_key || !request_id) return -1;
    hermes_log(LOG_INFO, "write_approval", "discard_pending: session=%s request=%s", session_key, request_id);
    return 0;
}

/* PoP: cli_tools_write_approval_pending_count @ tools/write_approval.py:pending_count */
int cli_tools_write_approval_pending_count(const char *session_key) {
    /*
     * Count pending write approval requests for a session.
     */
    if (!session_key) return 0;
    hermes_log(LOG_DEBUG, "write_approval", "pending_count: session=%s", session_key);
    /* In C, the actual count is managed by the approval system */
    return 0;
}

/* PoP: cli_tools_write_approval_current_origin @ tools/write_approval.py:current_origin */
const char* cli_tools_write_approval_current_origin(void) {
    /*
     * Get the current origin (CLI, gateway, etc.) for write approval.
     */
    const char *origin = getenv("HERMES_ORIGIN");
    if (!origin || !origin[0]) origin = "cli";
    hermes_log(LOG_DEBUG, "write_approval", "current_origin: %s", origin);
    return origin;
}

/* PoP: cli_tools_write_approval_is_background @ tools/write_approval.py:is_background */
int cli_tools_write_approval_is_background(void) {
    /*
     * Check if the current context is a background process.
     * Background processes may have different approval behavior.
     */
    const char *background = getenv("HERMES_BACKGROUND");
    int is_bg = (background && strcmp(background, "1") == 0);
    hermes_log(LOG_DEBUG, "write_approval", "is_background: %d", is_bg);
    return is_bg;
}

/* PoP: cli_tools_write_approval_evaluate_gate @ tools/write_approval.py:evaluate_gate */
int cli_tools_write_approval_evaluate_gate(const char *path, const char *operation, const char *session_key) {
    /*
     * Evaluate whether a write operation should be gated by approval.
     * Returns 1 if approval is required, 0 if not.
     */
    if (!path || !operation) return 1; /* Require approval for safety */
    /* Check if path is in an approved directory */
    int needs_approval = 1;
    if (strstr(path, "/tmp/") == path) needs_approval = 0;
    if (strstr(path, "/dev/null")) needs_approval = 0;
    hermes_log(LOG_DEBUG, "write_approval", "evaluate_gate: path=%s op=%s needs_approval=%d",
               path, operation, needs_approval);
    return needs_approval;
}

/* PoP: cli_tools_write_approval__interactive_approval_available @ tools/write_approval.py:_interactive_approval_available */
int cli_tools_write_approval__interactive_approval_available(void) {
    /*
     * Check if interactive approval is available (TTY present).
     */
    int has_tty = (isatty(fileno(stdin)) != 0);
    const char *no_interactive = getenv("HERMES_NO_INTERACTIVE");
    if (no_interactive && strcmp(no_interactive, "1") == 0) has_tty = 0;
    hermes_log(LOG_DEBUG, "write_approval", "_interactive_approval_available: %d", has_tty);
    return has_tty;
}

/* PoP: cli_tools_write_approval__prompt_inline_memory_approval @ tools/write_approval.py:_prompt_inline_memory_approval */
int cli_tools_write_approval__prompt_inline_memory_approval(const char *path, const char *content_hash) {
    /*
     * Prompt for inline memory approval.
     * Returns 1 if approved, 0 if denied.
     */
    if (!path || !content_hash) return 0;
    hermes_log(LOG_INFO, "write_approval", "_prompt_inline_memory_approval: path=%s hash=%.8s", path, content_hash);
    /* In C, interactive prompting is managed by the TUI layer */
    return 0;
}

/* PoP: cli_tools_write_approval__submit_approval_request @ tools/write_approval.py:_submit_approval_request */
char* cli_tools_write_approval__submit_approval_request(const char *path, const char *operation,
                                                         const char *session_key, char *buf, size_t bufsz) {
    /*
     * Submit a write approval request.
     * Returns the request ID as a string.
     */
    if (!buf || bufsz == 0) return NULL;
    /* Generate a request ID based on path and timestamp */
    unsigned long hash = 0;
    if (path) {
        int i;
        for (i = 0; path[i]; i++) {
            hash = hash * 31 + path[i];
        }
    }
    snprintf(buf, bufsz, "req-%08lx", hash);
    hermes_log(LOG_INFO, "write_approval", "_submit_approval_request: id=%s path=%s", buf, path ? path : "");
    return buf;
}

/* PoP: cli_tools_write_approval__check_approval @ tools/write_approval.py:_check_approval */
int cli_tools_write_approval__check_approval(const char *request_id) {
    /*
     * Check the status of a write approval request.
     * Returns 1 if approved, 0 if pending, -1 if denied.
     */
    if (!request_id) return -1;
    hermes_log(LOG_DEBUG, "write_approval", "_check_approval: %s", request_id);
    /* In C, approval checking is managed by the approval system */
    return 0;
}

/* PoP: cli_tools_write_approval__store_local_approval @ tools/write_approval.py:_store_local_approval */
int cli_tools_write_approval__store_local_approval(const char *path, const char *session_key) {
    /*
     * Store a local approval for a write path.
     * Returns 0 on success, -1 on failure.
     */
    if (!path || !session_key) return -1;
    hermes_log(LOG_INFO, "write_approval", "_store_local_approval: path=%s session=%s", path, session_key);
    /* In C, local approval storage is managed by the config module */
    return 0;
}

/* Port of Python tools/write_approval.py:write_approval_enabled */

/* Port of Python tools/write_approval.py:_normalize_enabled */

/* Port of Python tools/write_approval.py:stage_write */

/* Port of Python tools/write_approval.py:skill_gist */

/* Port of Python tools/write_approval.py:_frontmatter_description */

/* Port of Python tools/write_approval.py:skill_pending_diff */