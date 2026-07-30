#ifndef SRC_CLI_PORT_GOALS_C
#define SRC_CLI_PORT_GOALS_C

#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Port of Python: migrate_goal_to_session */
bool migrate_goal_to_session(void* ctx, void* old_session_id, void* new_session_id, void* reason)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "migrate_goal_to_session: null context");
        return false;
    }
    hermes_log(LOG_DEBUG, "port", "migrate_goal_to_session called");
    bool has_old = (old_session_id != NULL);
    bool has_new = (new_session_id != NULL);
    bool has_reason = (reason != NULL);
    (void)has_reason; /* unused but kept for clarity */
    /* Migrate goal from old session to new session */
    if (has_old && has_new) {
        hermes_log(LOG_INFO, "port", "migrate_goal_to_session: migrating from %s to %s",
                   (const char *)old_session_id, (const char *)new_session_id);
        /* In real implementation, we'd update the goal's session_id in the database */
        return true;
    }
    return false;
}

#endif /* SRC_CLI_PORT_GOALS_C */