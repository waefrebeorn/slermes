/*
 * noc_w34_egress_308cfb.h - C surface for upstream
 * gateway/relay/egress.py (gate w34_egress_308cfb).
 *
 * Gateway-side relay EGRESS AUTHORIZATION, as values. Two halves of one
 * security surface:
 *
 *   (a) DECLINE VISIBILITY. Recognise THAT the connector refused a
 *       destination - a definite (non-ambiguous) failure carrying the
 *       connector's decline code or its uniform decline sentence - report the
 *       decline text verbatim, and render the WARNING that a bool/None lane
 *       logs instead of swallowing. The sentence is matched on the
 *       "egress declined:" fragment only; the rest is deliberately
 *       uninformative and is never parsed.
 *   (b) TARGET ATTESTATION. Whether a send to a platform would egress over the
 *       relay at all, which chat ids this gateway can show a provenance for,
 *       and the verdict for one (platform, chat, thread) destination.
 *
 * Pure value -> value. Every runtime seam - the relay's fronted-platform
 * discovery, the gateway runner's adapter registry, the home-channel config,
 * the channel directory, the gateway-session origins, the native-token config
 * and the logger - arrives as a fixture VALUE. No socket, config file, runner,
 * connector or log sink is touched here.
 *
 * RelayRouteUnknown has no C exception, so the routing predicates answer the
 * string "unknown" where Python raises. A caller must treat "unknown" as a
 * FAULT (refuse), never fold it into "false" ("not relay-routed"), which would
 * skip authorization entirely.
 */
#ifndef NOC_W34_EGRESS_308CFB_H
#define NOC_W34_EGRESS_308CFB_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w34_egress_308cfb_is_egress_decline @ gateway/relay/egress.py:is_egress_decline
 * True when *result* is the connector REFUSING the destination: a dict that did
 * not succeed, is not ambiguous, and carries code=="egress_declined" or the
 * uniform decline text (case-insensitive) in its error. */
int w34_egress_308cfb_is_egress_decline(const json_t *result);

/* PoP: w34_egress_308cfb_declined_send @ gateway/relay/egress.py:declined_send
 * The same verdict over a projected SendResult: {"raw_response": <any>,
 * "error": <any>}. A dict raw_response is authoritative; otherwise the error
 * text is checked, and "ack lost" there is a transport outcome, never an
 * authorization refusal. */
int w34_egress_308cfb_declined_send(const json_t *result);

/* PoP: w34_egress_308cfb_decline_error @ gateway/relay/egress.py:decline_error
 * The connector's decline text, verbatim; "relay egress declined" when the
 * result carries none. */
json_t *w34_egress_308cfb_decline_error(const json_t *result);

/* PoP: w34_egress_308cfb_decline_log_line @ gateway/relay/egress.py:log_decline
 * The WARNING body log_decline emits, formatted and returned: the logger is the
 * seam, so what is compared is the message it would carry. */
json_t *w34_egress_308cfb_decline_log_line(const json_t *op, const json_t *chat_id,
                                           const json_t *result);

/* PoP: w34_egress_308cfb_is_missing_gateway_relay @ gateway/relay/egress.py:_is_missing_gateway_relay
 * ABSENCE vs FAULT for an ImportError: true only when it is a
 * ModuleNotFoundError whose name IS (a prefix of) gateway.relay.egress. A
 * nested dependency failing is a broken install, not "no relay here". */
int w34_egress_308cfb_is_missing_gateway_relay(int is_module_not_found, const char *name);

/* PoP: w34_egress_308cfb_has_live_native_adapter @ gateway/relay/egress.py:_has_live_native_adapter
 * The routing question for one platform, with the runner/config seams as
 * values: runner_present (a gateway runner exists), adapter_present (its
 * registry holds a non-None adapter for the platform), config_error (the
 * platform config read raised) and native_config (the platform's config
 * object, or null when the platform is absent from the config). Returns
 * "true" (native adapter shadows relay), "false" (no native adapter) or
 * "unknown" (the config read failed - RelayRouteUnknown). */
json_t *w34_egress_308cfb_has_live_native_adapter(int runner_present, int adapter_present,
                                                  int config_error, const json_t *native_config);

/* PoP: w34_egress_308cfb_relay_routed_platform @ gateway/relay/egress.py:relay_routed_platform
 * "true"/"false"/"unknown" for whether a send to in["platform"] egresses over
 * the relay connector. Reads in["fronted"] (the relay's fronted platform names),
 * in["route_unknown"] (fronted discovery raised) and the four
 * _has_live_native_adapter seams. The generic "relay" plane is always routed
 * and consults no seam. */
json_t *w34_egress_308cfb_relay_routed_platform(const json_t *in);

/* PoP: w34_egress_308cfb_session_ids @ gateway/relay/egress.py:_session_ids
 * The ids this gateway holds a session in, from the published session-origin
 * entries: [{"id": <any>, "thread_id": <any>}, ...]. A threaded origin is
 * f"{chat_id}:{thread_id}", and the parent is recovered from the STRUCTURED
 * thread_id field (suffix must match) - never by splitting the id. Sorted,
 * de-duplicated. */
json_t *w34_egress_308cfb_session_ids(const json_t *entries);

/* PoP: w34_egress_308cfb_attested_relay_targets @ gateway/relay/egress.py:attested_relay_targets
 * Chat ids this gateway can show a provenance for on in["platform"], as a
 * sorted array: the home channel plus the channel directory plus the
 * gateway-session origins, per in["sources"] = {"<platform>": {"home": <any>,
 * "directory": [{"id": <any>}], "sessions": [{"id", "thread_id"}]}}. On the
 * generic "relay" plane the union spans every fronted platform
 * (in["fronted"]). The string "unknown" stands in for RelayRouteUnknown
 * (fronted discovery failed on the relay plane). */
json_t *w34_egress_308cfb_attested_relay_targets(const json_t *in);

/* PoP: w34_egress_308cfb_is_unresolved_handle @ gateway/relay/egress.py:_is_unresolved_handle
 * True when the target is a NAME, not a resolved destination: an
 * "@"-prefixed target on telegram (and on nothing else). The exemption only
 * survives when no native credential can send it, so in["native_credential"]
 * (the config seam) or an explicitly supplied in["native_token"] must be
 * falsey; in["native_token_present"] says the caller supplied one. */
int w34_egress_308cfb_is_unresolved_handle(const json_t *in);

/* PoP: w34_egress_308cfb_authorize_relay_target @ gateway/relay/egress.py:authorize_relay_target
 * The guard itself: null when the destination may be named, the caller-facing
 * refusal string otherwise. in["platform"], in["chat_id"], in["thread_id"] are
 * the destination; the routing/fronted/sources/token seams are the same keys
 * the functions above read. A thread is part of the destination: an attested
 * parent does not vouch for a caller-supplied thread. */
json_t *w34_egress_308cfb_authorize_relay_target(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
