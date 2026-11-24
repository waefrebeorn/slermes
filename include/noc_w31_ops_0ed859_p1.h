/*
 * noc_w31_ops_0ed859_p1.h - C surface for the WEBHOOK PAYLOAD SHAPING partition
 * of upstream hermes_cli/web_routers/ops.py (gate w31_ops_0ed859_p1).
 *
 * What a subscription record looks like once the dashboard lists it, and which
 * create-webhook bodies the route refuses before anything is written: the
 * summary projection, the signature-pin rule (#84265), the normalised lookup
 * key, the name slug, and the direct-delivery target guard.
 *
 * Seams: the subscription JSON store (``_load_subscriptions`` /
 * ``_replace_subscription``), the webhook base URL, and the platform-enabled
 * read.  Every refusal returns as data -- {"ok","status","detail"} -- instead
 * of raising HTTPException.
 */
#ifndef NOC_W31_OPS_0ED859_P1_H
#define NOC_W31_OPS_0ED859_P1_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {name, route, base_url} -> the listed summary:
 * {name, description, events, deliver, deliver_only, prompt, script, skills,
 *  created_at, url, signature, secret_set, enabled}.
 * Only the literal ``enabled: false`` turns a route off (Python ``is not False``). */
json_t *w31_ops_0ed859_p1_webhook_route_summary(const json_t *req);

/* {requested, expected} -> {"signature": ...} or {}.
 * An explicit pick replaces the pin; a missing stored record pins "generic";
 * an omitted field keeps the stored pin verbatim. */
json_t *w31_ops_0ed859_p1_dashboard_route_signature(const json_t *req);

/* {subscriptions, name} -> {ok, status, detail, key?}.
 * 404 "No subscription named '<key>'" when the stripped/lowercased name is absent. */
json_t *w31_ops_0ed859_p1_webhook_key_in(const json_t *req);

/* {name} -> {ok, status, detail, name?}: strip, lowercase, spaces -> '-', then
 * ^[a-z0-9][a-z0-9_-]*$ (400 "Invalid name. ..."). */
json_t *w31_ops_0ed859_p1_webhook_name_verdict(const json_t *req);

/* {deliver, deliver_only} -> {ok, status, detail}: deliver_only with the "log"
 * target is refused (400). */
json_t *w31_ops_0ed859_p1_webhook_deliver_verdict(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
