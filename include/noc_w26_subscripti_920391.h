/*
 * noc_w26_subscripti_920391.h - C surface for upstream
 * plugins/teams_pipeline/subscriptions.py (gate w26_subscripti_920391).
 *
 * Microsoft Graph subscription upkeep.  The store and the Graph client are
 * seams: what is ported is the pure part -- the configured clientState, the
 * second-precision UTC expiration stamp Graph needs, and the "is this
 * subscription ours?" test the maintainer gates every remote row on.
 */
#ifndef NOC_W26_SUBSCRIPTI_920391_H
#define NOC_W26_SUBSCRIPTI_920391_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MSGRAPH_WEBHOOK_CLIENT_STATE is the environment seam this one reads. */
#define W26_SUBSCRIPTI_920391_ENV "MSGRAPH_WEBHOOK_CLIENT_STATE"

/* expected_client_state(raw): str -> stripped non-empty string or null.
 * A null/absent `raw` reads W26_SUBSCRIPTI_920391_ENV, as the live function does. */
json_t *w26_subscripti_920391_expected_client_state(const json_t *raw);

/* utc_timestamp(hours_from_now, base): second-precision UTC ISO with a Z
 * suffix, Graph's expirationDateTime format.  `base` is an ISO-8601 string. */
json_t *w26_subscripti_920391_utc_timestamp(const json_t *hours_from_now, const json_t *base);

/* is_managed_subscription(known_ids, payload, expected_client_state_value):
 * the store lookup is replaced by the list of ids the store already knows. */
json_t *w26_subscripti_920391_is_managed_subscription(const json_t *known_ids,
                                                     const json_t *payload,
                                                     const json_t *expected_client_state_value);

#ifdef __cplusplus
}
#endif
#endif
