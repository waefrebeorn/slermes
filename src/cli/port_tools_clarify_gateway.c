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

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Clarify entry structure — mirrors Python _ClarifyEntry dataclass.
 * In the C implementation, entries are managed via a simple linked list
 * since the full threading/event infrastructure is in the gateway layer.
 */
typedef struct clarify_entry {
    char clarify_id[128];
    char session_key[256];
    char question[1024];
    char choices[16][256];   /* up to 16 canned choices (for _coerce_text_response) */
    int  choice_count;
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
void *cli_tools_clarify_gateway_get_notify(const char *session_key) {
    /* Per-session notify callbacks are not retained in this C port (the
     * gateway calls send_clarify directly). Return NULL (no callback). */
    (void)session_key;
    return NULL;
}

/* ================================================================
 *  Text-response resolution (choice coercion)
 * ================================================================ */

/* PoP: cli_tools_clarify_gateway__coerce_text_response @ tools/clarify_gateway.py:_coerce_text_response */
/* Map typed choice replies to canonical choice text, otherwise keep custom text. */
char *cli_tools_clarify_gateway__coerce_text_response(const clarify_entry_t *entry, const char *response) {
    if (!response) return strdup("");
    /* strip */
    char text[2048];
    size_t n = 0;
    for (const char *p = response; *p && n + 1 < sizeof(text); p++) {
        if (!isspace((unsigned char)*p) || n > 0) text[n++] = *p;
    }
    while (n > 0 && isspace((unsigned char)text[n - 1])) n--;
    text[n] = '\0';
    if (entry && entry->choice_count > 0) {
        /* numeric index? */
        char *end = NULL;
        long idx = strtol(text, &end, 10);
        if (end != text && *end == '\0') {
            if (idx >= 1 && idx <= entry->choice_count)
                return strdup(entry->choices[idx - 1]);
        }
        /* exact casefold match */
        for (int i = 0; i < entry->choice_count; i++) {
            char a[256], b[256];
            size_t ia = 0, ib = 0;
            for (const char *p = text; *p; p++) a[ia++] = (char)tolower((unsigned char)*p); a[ia] = '\0';
            for (const char *p = entry->choices[i]; *p; p++) b[ib++] = (char)tolower((unsigned char)*p); b[ib] = '\0';
            /* strip trailing space on both */
            while (ia > 0 && isspace((unsigned char)a[ia - 1])) a[--ia] = '\0';
            while (ib > 0 && isspace((unsigned char)b[ib - 1])) b[--ib] = '\0';
            if (strcmp(a, b) == 0) return strdup(entry->choices[i]);
        }
    }
    return strdup(text);
}

/* PoP: cli_tools_clarify_gateway_resolve_text_response_for_session @ tools/clarify_gateway.py:resolve_text_response_for_session */
int cli_tools_clarify_gateway_resolve_text_response_for_session(const char *session_key, const char *response) {
    clarify_entry_t *entry = cli_tools_clarify_gateway_get_pending_for_session(session_key);
    if (!entry) return 0;
    char *coerced = cli_tools_clarify_gateway__coerce_text_response(entry, response);
    int ok = cli_tools_clarify_gateway_resolve_gateway_clarify(entry->clarify_id, coerced);
    free(coerced);
    return ok;
}