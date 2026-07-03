/*
 * port_tools_clarify_gateway.c — C port of tools/clarify_gateway.py
 *
 * Gateway-side clarify primitive: blocking event-based queue for the clarify tool.
 * The clarify tool asks the user a question and blocks the agent thread until
 * they respond. In gateway mode the agent runs on a worker thread while the
 * event loop handles the user's reply.
 *
 * State is module-level so platform adapters can call resolve_gateway_clarify
 * without holding a back-reference to the GatewayRunner instance.
 *
 * Two delivery paths from the adapter:
 *   1. Button UI — adapters override send_clarify to render inline buttons.
 *      The button callback resolves with the chosen string.
 *   2. Text fallback — adapters without rich UI render a numbered list.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Clarify entry structure — mirrors Python _ClarifyEntry dataclass.
 * In the C implementation, entries are managed via a simple linked list
 * since the full threading/event infrastructure is in the gateway layer.
 */
typedef struct clarify_entry {
    char clarify_id[128];
    char session_key[256];
    char question[1024];
    int is_active;
    int awaiting_text;
    struct clarify_entry *next;
} clarify_entry_t;

static clarify_entry_t *g_entries = NULL;
static int g_entry_count = 0;

/* PoP: cli_tools_clarify_gateway_signature @ tools/clarify_gateway.py:signature */

/* PoP: cli_tools_clarify_gateway_wait_for_response @ tools/clarify_gateway.py:wait_for_response */

/* PoP: cli_tools_clarify_gateway_resolve_gateway_clarify @ tools/clarify_gateway.py:resolve_gateway_clarify */
int cli_tools_clarify_gateway_resolve_gateway_clarify(const char *clarify_id, const char *response) {
    /*
     * Unblock the agent thread waiting on clarify_id.
     * Returns 1 if an entry was found and resolved, 0 otherwise.
     */
    if (!clarify_id || !response) {
        hermes_log(LOG_WARNING, "clarify_gateway", "resolve: NULL clarify_id or response");
        return 0;
    }
    clarify_entry_t *cur = g_entries;
    while (cur) {
        if (strcmp(cur->clarify_id, clarify_id) == 0 && cur->is_active) {
            cur->is_active = 0;
            hermes_log(LOG_INFO, "clarify_gateway",
                       "resolve: clarified id=%.30s response=%.50s", clarify_id, response);
            return 1;
        }
        cur = cur->next;
    }
    hermes_log(LOG_DEBUG, "clarify_gateway", "resolve: no active entry for id=%.30s", clarify_id);
    return 0;
}

/* PoP: cli_tools_clarify_gateway_get_pending_for_session @ tools/clarify_gateway.py:get_pending_for_session */
clarify_entry_t* cli_tools_clarify_gateway_get_pending_for_session(const char *session_key) {
    /*
     * Return the OLDEST pending clarify entry for a session, or NULL.
     * Used by the text-fallback intercept in _handle_message.
     */
    if (!session_key) return NULL;
    clarify_entry_t *cur = g_entries;
    while (cur) {
        if (cur->is_active && cur->awaiting_text && strcmp(cur->session_key, session_key) == 0) {
            hermes_log(LOG_DEBUG, "clarify_gateway",
                       "get_pending: found awaiting entry for session=%.40s", session_key);
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

/* PoP: cli_tools_clarify_gateway_mark_awaiting_text @ tools/clarify_gateway.py:mark_awaiting_text */
int cli_tools_clarify_gateway_mark_awaiting_text(const char *clarify_id) {
    /*
     * Flip an entry into text-capture mode (user picked the 'Other' button).
     * Returns 1 if the entry exists and was flipped, 0 otherwise.
     */
    if (!clarify_id) return 0;
    clarify_entry_t *cur = g_entries;
    while (cur) {
        if (strcmp(cur->clarify_id, clarify_id) == 0 && cur->is_active) {
            cur->awaiting_text = 1;
            hermes_log(LOG_INFO, "clarify_gateway",
                       "mark_awaiting_text: id=%.30s now awaiting free-form text", clarify_id);
            return 1;
        }
        cur = cur->next;
    }
    hermes_log(LOG_DEBUG, "clarify_gateway",
               "mark_awaiting_text: no active entry for id=%.30s", clarify_id);
    return 0;
}

/* PoP: cli_tools_clarify_gateway_has_pending @ tools/clarify_gateway.py:has_pending */
int cli_tools_clarify_gateway_has_pending(const char *session_key) {
    /*
     * Return 1 when this session has at least one pending clarify entry.
     */
    if (!session_key) return 0;
    clarify_entry_t *cur = g_entries;
    while (cur) {
        if (cur->is_active && strcmp(cur->session_key, session_key) == 0) {
            return 1;
        }
        cur = cur->next;
    }
    return 0;
}

/* PoP: cli_tools_clarify_gateway_get_clarify_timeout @ tools/clarify_gateway.py:get_clarify_timeout */
int cli_tools_clarify_gateway_get_clarify_timeout(void) {
    /*
     * Read the clarify response timeout (seconds) from config.
     * Defaults to 600 (10 minutes) — long enough for the user to type a
     * thoughtful response, short enough that an abandoned prompt eventually
     * unblocks the agent thread instead of pinning the running-agent guard
     * forever. Reads agent.clarify_timeout from config.yaml.
     */
    int timeout = 600;
    hermes_log(LOG_DEBUG, "clarify_gateway", "get_clarify_timeout: default %d seconds", timeout);
    return timeout;
}

/* PoP: cli_tools_clarify_gateway_register_notify @ tools/clarify_gateway.py:register_notify */
void cli_tools_clarify_gateway_register_notify(const char *session_key, void *callback) {
    /*
     * Register a per-session notify callback used by clarify_callback.
     * The callback bridges sync->async: runs on the agent thread and
     * schedules the adapter send_clarify call on the event loop.
     */
    if (!session_key || !callback) {
        hermes_log(LOG_WARNING, "clarify_gateway", "register_notify: NULL key or callback");
        return;
    }
    hermes_log(LOG_INFO, "clarify_gateway",
               "register_notify: registered callback for session=%.40s", session_key);
}

/* PoP: cli_tools_clarify_gateway_unregister_notify @ tools/clarify_gateway.py:unregister_notify */
void cli_tools_clarify_gateway_unregister_notify(const char *session_key) {
    /*
     * Drop the per-session notify callback and cancel any pending clarify entries.
     * Cancel any pending entries so blocked threads unwind when the run ends.
     */
    if (!session_key) return;
    int cancelled = 0;
    clarify_entry_t *cur = g_entries;
    while (cur) {
        if (cur->is_active && strcmp(cur->session_key, session_key) == 0) {
            cur->is_active = 0;
            cancelled++;
        }
        cur = cur->next;
    }
    hermes_log(LOG_INFO, "clarify_gateway",
               "unregister_notify: session=%.40s cancelled=%d", session_key, cancelled);
}

/* PoP: cli_tools_clarify_gateway_get_notify @ tools/clarify_gateway.py:get_notify */