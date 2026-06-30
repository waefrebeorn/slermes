#ifndef SRC_CLI_PORT_MODEL_SWITCH_C
#define SRC_CLI_PORT_MODEL_SWITCH_C

#include "hermes.h"
#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Port of Python: resolve_persist_behavior */
bool resolve_persist_behavior(void* ctx, void* is_global, void* is_session)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "resolve_persist_behavior: null context");
        return false;
    }
    hermes_log(LOG_DEBUG, "port", "resolve_persist_behavior called");
    if (is_global) {
        hermes_log(LOG_DEBUG, "port", "resolve_persist_behavior: is_global is set");
    }
    if (is_session) {
        hermes_log(LOG_DEBUG, "port", "resolve_persist_behavior: is_session is set");
    }
    /* Determine persist behavior based on flags */
    bool persist_global = (is_global != NULL);
    bool persist_session = (is_session != NULL);
    bool result = persist_global || persist_session;
    hermes_log(LOG_DEBUG, "port", "resolve_persist_behavior: global=%d session=%d -> %d",
               persist_global, persist_session, result);
    return result;
}

#endif /* SRC_CLI_PORT_MODEL_SWITCH_C */