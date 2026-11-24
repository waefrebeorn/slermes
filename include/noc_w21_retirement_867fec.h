#ifndef NOC_W21_RETIREMENT_867FEC_H
#define NOC_W21_RETIREMENT_867FEC_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/housekeeping/_retirement.py (gate w21_retirement_867fec).
 *
 * `prune_retired_tombstones` is a module function over an open SQLite
 * connection. The port keeps the whole decision -- which tombstones get a
 * `retired_at` stamp this pass, which keys the four DELETEs remove, and the two
 * `_reclaim_expired` sweeps with their cutoff -- and takes the `state_meta`
 * rows as data instead of executing SQL (the DB seam). Identity pointers use
 * the live `identity_key` construction, so the reviewed digest is reproduced,
 * not approximated.
 */

/* PoP: w21_retirement_867fec_constants @ persistence/sqlite/housekeeping/_retirement.py:prune_retired_tombstones
 * The key prefixes and horizons the prune is built from:
 *   {"admission","result","worker","identity","retired","route_fence","retry_horizon"} */
json_t *w21_retirement_867fec_constants(void);

/* PoP: w21_retirement_867fec_identity_key @ persistence/sqlite/conversation/_store/terminal.py:identity_key
 * IDENTITY_PREFIX + sha256(json.dumps({'target':..., 'payload':{'principal':...,'request':...}},
 * sort_keys=True, separators=(',',':'))). Caller frees. */
char *w21_retirement_867fec_identity_key(const json_t *principal, const json_t *target,
                                         const json_t *request);

/* PoP: w21_retirement_867fec_prune_plan @ persistence/sqlite/housekeeping/_retirement.py:prune_retired_tombstones
 * One prune pass over `rows` = [{"key":<str>,"value":<json text>}, ...]:
 *   {"stamped":[key...], "deleted":[key...], "reclaim":[[prefix,cutoff],...]}
 * `now` is the clock seam (time.time()); `has_older_than` false applies the
 * module default RETRY_HORIZON_S. `stamped` keeps row order, `deleted` is
 * sorted. Deleting an admission also drops its compacted result and its
 * identity pointer (as the `key IN (?,?,?)` DELETE does). */
json_t *w21_retirement_867fec_prune_plan(const json_t *rows, double now,
                                         int has_older_than, double older_than);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_RETIREMENT_867FEC_H */
