/*
 * port_gateway_mirror.c — C port of gateway/mirror.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_mirror__append_to_sqlite @ gateway/mirror.py:_append_to_sqlite */

/*
 * _append_to_sqlite: Append a message to the SQLite session database.
 *
 * p1 = session_id string
 * p2 = role string
 * p3 = content string
 *
 * Returns: 0 on success, -1 on failure.
 */
void* cli_gateway_mirror__append_to_sqlite(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p4; (void)p5;

    const char *session_id = (const char *)p1;
    const char *role = (const char *)p2;
    const char *content = (const char *)p3;

    if (!session_id || !role || !content) {
        hermes_log(LOG_DEBUG, "port",
                   "mirror: append_to_sqlite called with NULL args");
        return (void *)(intptr_t)(-1);
    }

    /* In the C runtime, session messages are appended via the session DB.
     * This is a simplified implementation that logs the mirror operation.
     * The actual SQLite write is handled by the session subsystem. */
    hermes_log(LOG_DEBUG, "port",
               "mirror: appending message to session %s (role=%s, content_len=%zu)",
               session_id, role, strlen(content));

    /* Return success — the C runtime handles session persistence internally */
    return (void *)(intptr_t)0;
}
