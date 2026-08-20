/* Slermes C port — gateway/relay/ws_transport.py (remaining gaps)
 *
 * Faithful port of _disconnect_drain_grace_s: the effective drain grace
 * clamped to the caller's disconnect budget. Pure arithmetic + one env read
 * (no async, no subprocess). Mirrors gateway/relay/ws_transport.py.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "hermes_json.h"

/* Constants mirrored from gateway/relay/ws_transport.py. */
#define RL_DISCONNECT_DRAIN_GRACE_S      5.0
#define RL_TEARDOWN_AWAIT_TIMEOUT_S      1.0
#define RL_ADAPTER_DISCONNECT_TIMEOUT_DEF 5.0

/* Read the runner's adapter-disconnect budget the same way
 * gateway/run.py:_adapter_disconnect_timeout_secs reads it (same env var,
 * same default). */
static double rl_env_disconnect_budget_s(void)
{
    double budget = RL_ADAPTER_DISCONNECT_TIMEOUT_DEF;
    const char *raw = getenv("HERMES_GATEWAY_ADAPTER_DISCONNECT_TIMEOUT");
    if (raw && *raw) {
        char *end = NULL;
        double v = strtod(raw, &end);
        if (end != raw && *end == '\0') budget = (v < 0.0) ? 0.0 : v;
    }
    return budget;
}

/* PoP: relay_ws_transport_disconnect_drain_grace_s @ gateway/relay/ws_transport.py:_disconnect_drain_grace_s */
/* Effective drain grace: clamped to the caller's disconnect budget.
 * `budget_s` < 0 means "not supplied" — read the env default instead.
 * Reserved = 3 * TEARDOWN_AWAIT_TIMEOUT_S + 0.5 (three sequential teardown
 * awaits plus a small margin). Returns max(0, min(GRACE, budget - reserved)). */
double relay_ws_transport_disconnect_drain_grace_s(double budget_s)
{
    double budget = (budget_s < 0.0) ? rl_env_disconnect_budget_s() : budget_s;
    if (budget < 0.0) budget = 0.0;
    double reserved = 3.0 * RL_TEARDOWN_AWAIT_TIMEOUT_S + 0.5;
    double grace = RL_DISCONNECT_DRAIN_GRACE_S;
    double effective = (budget > reserved) ? (budget - reserved) : 0.0;
    return (effective < grace) ? effective : grace;
}
