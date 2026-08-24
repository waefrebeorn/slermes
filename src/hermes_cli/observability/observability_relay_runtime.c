/*
 * observability_relay_runtime.c — organization twin of
 * hermes_cli/observability/relay_runtime.py.
 *
 * The Python module is a compatibility alias: it re-exports
 * agent.relay_runtime so legacy imports keep working during migration.
 * The C port of the relay runtime lives in src/gateway/relay/relay_runtime.c;
 * this module declares that mapping and provides an accessor for new callers.
 */

#include <stddef.h>

/* PoP: obs_relay_runtime_module @ agent/relay_runtime.py (compat alias of hermes_cli/observability/relay_runtime.py) */
const char *obs_relay_runtime_module(void) {
    return "agent.relay_runtime";
}
