/*
 * noc_w26_webserveri_efe232.h - C surface for upstream
 * hermes_cli/web_server_idle_exit.py (gate w26_webserveri_efe232).
 *
 * The liveness half of an SSH-isolated ``hermes serve`` backend (#101626): the
 * accepted-WebSocket counter, the "is a turn provably running" probe's bool
 * view, the exit predicate (nil clients for the grace window AND no turn), and
 * the tunnel ping budgets.  The clock, the session table and the ledgers are
 * fixed inputs here.
 */
#ifndef NOC_W26_WEBSERVERI_EFE232_H
#define NOC_W26_WEBSERVERI_EFE232_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W26_WEBSERVERI_EFE232_IDLE_GRACE_S    900.0
#define W26_WEBSERVERI_EFE232_PING_INTERVAL_S  60.0
#define W26_WEBSERVERI_EFE232_PING_TIMEOUT_S  600.0

/* {"start": float, "ops": [{"op": "open"|"close", "now": float}], "now": float}
 * -> {"live": int, "idle_for": float}   (web_server_idle_exit.IdleClientTracker) */
json_t *w26_webserveri_efe232_tracker(const json_t *in);

/* {"session": {...}} -> bool   (web_server_idle_exit._session_work_in_flight;
 * a thread is given as {"alive": bool}) */
json_t *w26_webserveri_efe232_session_work_in_flight(const json_t *in);

/* {"live": int, "last_client_at": float, "now": float, "grace_s": float,
 *  "probe": "true"|"false"|"none"} -> bool
 * (web_server_idle_exit.should_exit_idle) */
json_t *w26_webserveri_efe232_should_exit_idle(const json_t *in);

/* {"ledger": str|null} -> bool|null   (web_server_idle_exit.turn_in_flight) */
json_t *w26_webserveri_efe232_turn_in_flight_view(const json_t *in);

/* The module's literal budgets. */
json_t *w26_webserveri_efe232_constants(void);

#ifdef __cplusplus
}
#endif
#endif
