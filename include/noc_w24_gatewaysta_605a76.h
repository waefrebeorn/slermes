/*
 * noc_w24_gatewaysta_605a76.h - C surface for upstream
 * hermes_cli/gateway_status_notes.py (gate w24_gatewaysta_605a76).
 *
 * The notes ``hermes gateway status`` prints beside its verdict, decided from
 * values instead of from the machine: the incompatible-owner advisory, the four
 * lines that accompany it, and the "standalone by config" line.  Discovery (the
 * endpoint probe and its timeout), the Hermes home, the profile roster and the
 * process scan are seams -- each arrives here as a value, so nothing is dialled,
 * read or scanned.
 */
#ifndef NOC_W24_GATEWAYSTA_605A76_H
#define NOC_W24_GATEWAYSTA_605A76_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _incompatible_owner_verdict(): *observed* is the discovery verdict
 * ({"state": ..., "reason_code": ...}) or null when discovery reported nothing;
 * *raises* models discovery raising OSError/ValueError (a bad home), which the
 * module swallows.  -> "incompatible: <reason_code or 'unknown'>" or null. */
json_t *w24_gatewaysta_605a76_verdict(const json_t *observed, int raises);

/* report_incompatible_owner(pids) as text: the lines it would print, or null
 * when the gateway can serve this Hermes (nothing is printed).  Empty or null
 * *pids* names a service-managed gateway. */
json_t *w24_gatewaysta_605a76_report_lines(const json_t *observed, int raises,
                                           const json_t *pids);

/* print_standalone_by_config() as text: the profiles that opted out of the host
 * multiplexer by config, or null when the active profile is not "default" or
 * nothing opted out.  *roster* is the installed set (include_standalone=True),
 * *served* the set the host gateway serves. */
json_t *w24_gatewaysta_605a76_standalone_note(const json_t *active,
                                              const json_t *roster,
                                              const json_t *served);

#ifdef __cplusplus
}
#endif
#endif
