/*
 * noc_w28_messaging_ede80b_p3.h — C surface for upstream
 * hermes_cli/web_routers/messaging.py (gate w28_messaging_ede80b_p3).
 *
 * The Channels page: env-var metadata derivation, CSV allowlist counting, the
 * per-platform payload/state ladder, the auth-posture verdict, the multiplex
 * port-binding conflict reason and the /test verdict ladder.
 *
 * Seams (values handed in, not reached for): the .env / config.yaml /
 * gateway_state.json readers, the platform enablement arm and the live gateway
 * liveness probe.  The credential preview of an env row is a display concern:
 * the already-shaped rows arrive in "env_vars" and are passed through.
 *
 * Every entry point returns an envelope object:
 *   {"ok": true,  "value": <json>}
 *   {"ok": false, "status": <int>, "detail": <string>}
 */
#ifndef NOC_W28_MESSAGING_EDE80B_P3_H
#define NOC_W28_MESSAGING_EDE80B_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* (key, info-or-null) -> the dashboard row metadata. */
json_t *w28_messaging_ede80b_p3_env_info(const json_t *key, const json_t *info);

/* " a, b ,, c " -> 3. */
json_t *w28_messaging_ede80b_p3_count_csv(const json_t *value);

/* The platform payload: input keys entry, env_on_disk, scoped, runtime,
 * gateway_running, enabled, configured, home_channel, mirrors, retained_state,
 * relay_disabled, relay_fronted, relay_exclusive, env_vars. */
json_t *w28_messaging_ede80b_p3_platform_state(const json_t *in);

/* Auth posture for a connected platform: input keys platform_id, entry,
 * allowed_env, allow_all_env, env, dm_policy, approved, pending.  value is null
 * when the platform has no per-user authorization surface. */
json_t *w28_messaging_ede80b_p3_posture(const json_t *in);

/* The 409 reason enabling a mirrored platform on a secondary profile is
 * pointless, or null: input keys platform_id, requested_profile, active_profile,
 * mirrors, multiplexes. */
json_t *w28_messaging_ede80b_p3_multiplex_conflict(const json_t *in);

/* The /api/messaging/platforms/{id}/test verdict: input keys entry, payload,
 * posture. */
json_t *w28_messaging_ede80b_p3_test_verdict(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
