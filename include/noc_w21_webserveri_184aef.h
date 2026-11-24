#ifndef NOC_W21_WEBSERVERI_184AEF_H
#define NOC_W21_WEBSERVERI_184AEF_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/web_server_idle_proof.py (gate w21_webserveri_184aef):
 * the diagnostic work snapshot that says whether a Desktop-pooled
 * `hermes serve` child has anything left to do.
 *
 * Both probes are upstream arguments (`turn_probe`, `input_probe`); the port
 * takes the value a probe answered with, because the live ledgers they read
 * (session table, cron ledger, request/approval tables) are database seams:
 *   turn    -- absent/None: probe unavailable; bool: in flight or not;
 *              a string: in flight, and the busy ledger's name
 *   pending -- absent/None: probe unavailable; number: prompts waiting on a human
 *
 * Unreadable state is indeterminate, never idle. This snapshot alone is NOT
 * permission to retire: only `backend_retirement.RetirementFence.prepare`
 * freezes admission before taking it. */

/* PoP: w21_webserveri_184aef_idle_proof @ hermes_cli/web_server_idle_proof.py:idle_proof
 * `{"idle": True | False | None, "reason": str | None}` plus `"detail"` naming
 * the busy ledger:
 *   turn unavailable -> {"idle":null,"reason":"turn_probe_unavailable"}
 *   turn truthy      -> {"idle":false,"reason":"turn_in_flight","detail":<str|None>}
 *   pending unavailable -> {"idle":null,"reason":"input_probe_unavailable"}
 *   pending truthy   -> {"idle":false,"reason":"awaiting_human_input"}
 *   else             -> {"idle":true,"reason":null} */
json_t *w21_webserveri_184aef_idle_proof(const json_t *turn, const json_t *pending);

/* PoP: w21_webserveri_184aef_pending_human_input @ hermes_cli/web_server_idle_proof.py:pending_human_input
 * `open_request_count() + pending_gateway_approval_count()`, or null when a
 * ledger cannot be read (`unavailable`, the seam for the failing probe). */
json_t *w21_webserveri_184aef_pending_human_input(const json_t *server_requests,
                                                  const json_t *approvals,
                                                  int unavailable);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_WEBSERVERI_184AEF_H */
