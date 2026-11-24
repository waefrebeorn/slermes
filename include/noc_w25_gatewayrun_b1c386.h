/*
 * noc_w25_gatewayrun_b1c386.h - C surface for upstream
 * hermes_cli/gateway_runtime_multiplex.py (gate w25_gatewayrun_b1c386).
 *
 * Which daemon a named-profile client must start when nothing serves its home.
 * The profile layout on disk, ``config.yaml`` (``gateway.standalone`` /
 * ``gateway.multiplex_profiles``), the multiplexer's recorded
 * ``gateway_state.json`` and the profile registry are seams; the facts they
 * yield decide whether the root's multiplexer owns the home or a per-profile
 * gateway must be started instead.
 */
#ifndef NOC_W25_GATEWAYRUN_B1C386_H
#define NOC_W25_GATEWAYRUN_B1C386_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* hermes_cli.profiles.normalize_profile_name: strip, the ``default`` alias
 * matched case-insensitively, every other id lowercased.  A non-string name, or
 * one that strips to nothing, raises ValueError (reported as {"__error__":...}). */
json_t *w25_gatewayrun_b1c386_normalize_name(const json_t *name);

/* gateway_multiplex_mode.settled_explicit_flag: an explicit ``true`` is final;
 * everything falsy (unset, the retired ``false``) settles as None. */
json_t *w25_gatewayrun_b1c386_settled_flag(const json_t *value);

/* multiplexer_serves_home's verdict: named profile, boot verdict
 * (true/false/null), recorded served_profiles (list/null) and the home's own
 * profile name -> bool.  A standalone secondary is never served; an explicit
 * verdict is final; otherwise the last recorded served set decides. */
json_t *w25_gatewayrun_b1c386_serves_home(const json_t *named,
                                          const json_t *verdict,
                                          const json_t *served,
                                          const json_t *home_name);

/* implied_host_root's verdict: a named profile is served unless its own config
 * is a standalone one (verdict false); no recorded verdict keeps it served. */
json_t *w25_gatewayrun_b1c386_implied_serves(const json_t *named,
                                             const json_t *verdict);

#ifdef __cplusplus
}
#endif
#endif
