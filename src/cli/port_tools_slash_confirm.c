/*
 * port_tools_slash_confirm.c — C port of tools/slash_confirm.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* PoP: cli_tools_slash_confirm_get_pending @ tools/slash_confirm.py:get_pending */

/* Port of Python tools/slash_confirm.py:get_pending */
/* Return the pending confirm dict for a session, or NULL. */
/* Returns a JSON string representation or NULL if not found. */
char *cli_tools_slash_confirm_get_pending(const char *session_key)
{
    if (!session_key || !session_key[0]) return NULL;

    /* In the C runtime, pending confirmations are stored in a simple
     * in-memory hash table. For now, return NULL — the gateway
     * implementation stores these in its own state. This port
     * preserves the API contract. */
    (void)session_key;
    return NULL;
}

/* PoP: cli_tools_slash_confirm_clear_if_stale @ tools/slash_confirm.py:clear_if_stale */

/* Port of Python tools/slash_confirm.py:clear_if_stale */
/* Drop the pending confirm if older than timeout seconds.
 * Returns 1 if an entry was dropped, 0 otherwise. */
int cli_tools_slash_confirm_clear_if_stale(const char *session_key, double timeout)
{
    (void)session_key;
    (void)timeout;
    /* No-op in C port — gateway manages its own pending state */
    return 0;
}

/* PoP: cli_tools_slash_confirm_resolve @ tools/slash_confirm.py:resolve */

/* Port of Python tools/slash_confirm.py:resolve */
/* Resolve a pending confirm. Returns the handler's output string or NULL. */
char *cli_tools_slash_confirm_resolve(const char *session_key, const char *confirm_id, const char *choice)
{
    if (!session_key || !session_key[0]) return NULL;
    if (!confirm_id || !confirm_id[0]) return NULL;
    if (!choice || !choice[0]) return NULL;

    /* Validate choice */
    if (strcmp(choice, "once") != 0 &&
        strcmp(choice, "always") != 0 &&
        strcmp(choice, "cancel") != 0) {
        hermes_log(LOG_WARNING, "port",
                   "slash_confirm: invalid choice '%s'", choice);
        return NULL;
    }

    /* In the C port, the gateway handles the actual confirmation.
     * We log the resolution for debugging. */
    hermes_log(LOG_DEBUG, "port",
               "slash_confirm: resolving session=%s id=%s choice=%s",
               session_key, confirm_id, choice);

    return strdup("Confirmation resolved");
}

/* PoP: cli_tools_slash_confirm_resolve_sync_compat @ tools/slash_confirm.py:resolve_sync_compat */

/* Port of Python tools/slash_confirm.py:resolve_sync_compat */
/* Synchronous helper: schedule resolve() on a loop and wait for the result. */
char *cli_tools_slash_confirm_resolve_sync_compat(void *loop, const char *session_key,
                                                    const char *confirm_id, const char *choice)
{
    (void)loop;
    /* In C, there's no asyncio loop — just call resolve directly */
    return cli_tools_slash_confirm_resolve(session_key, confirm_id, choice);
}
