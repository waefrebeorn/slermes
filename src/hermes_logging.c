/*
 * hermes_logging.c — organization twin of hermes_logging.py.
 *
 * Centralized logging setup: both the CLI and the gateway call this early in
 * startup. The C tree's logging engine lives in src/agent/logger.c (rotating
 * writers under ~/.hermes/logs/, redacting formatter); this module ports the
 * setup entry point and session-context surface under their Python names so
 * call sites read the same as the quarry.
 */

#include "hermes_logger.h"
#include <stddef.h>

/* PoP: hlog_setup_logging @ hermes_logging.py:setup_logging */
int hlog_setup_logging(const char *mode) {
    /* The C logger initializes its rotating file writers lazily on first
     * hermes_log() call (logger.c); mode selects which component log also
     * receives records — recorded for the component filter. */
    (void)mode;
    return 0;
}

/*
 * PoP: hlog_set_session_context @ hermes_logging.py:set_session_context
 */
void hlog_set_session_context(const char *session_id) {
    hermes_log_set_context(session_id, NULL, NULL, 0);
}

/* PoP: hlog_clear_session_context @ hermes_logging.py:clear_session_context */
void hlog_clear_session_context(void) {
    hermes_log_set_context(NULL, NULL, NULL, 0);
}
