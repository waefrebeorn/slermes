#ifndef NOC_W20_CREDENTIAL_AF0C5F_H
#define NOC_W20_CREDENTIAL_AF0C5F_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/credential_pool_selection.py (gate w20_credential_af0c5f).
 *
 * The mixin's pure halves: which entries serve an endpoint, the strategy pick,
 * the recorded pick (request_count + round-robin re-prioritisation), the
 * unlocked selection flow and peek.  The live pool object, its lock, the token
 * refresh and auth.json persistence stay upstream; entries arrive as
 * {"id","base_url"?,"runtime_base_url"?,"request_count"?,"priority"?} and come
 * back as the {"id","priority","request_count"} triple. */

/* PoP: w20_credential_af0c5f_serving @ agent/credential_pool_selection.py:_serving
 * Entries that serve `endpoint_url` (all of them when it is null); ids in order. */
json_t *w20_credential_af0c5f_serving(const json_t *entries, const char *endpoint_url);

/* PoP: w20_credential_af0c5f_strategy_pick @ agent/credential_pool_selection.py:_strategy_pick
 * random -> entries[rng_index % n] (the rng seam), least_used -> first minimum
 * request_count, otherwise the first entry. */
json_t *w20_credential_af0c5f_strategy_pick(const char *strategy, const json_t *entries, int rng_index);

/* PoP: w20_credential_af0c5f_record_pick @ agent/credential_pool_selection.py:_record_pick
 * {"entry": triple, "entries": [triples], "persisted": bool}.  count=false skips
 * the request_count bump; round_robin (with >1 entry) re-orders and re-prioritises. */
json_t *w20_credential_af0c5f_record_pick(const char *strategy, const json_t *entries,
                                          const char *picked_id, int count);

/* PoP: w20_credential_af0c5f_select_unlocked @ agent/credential_pool_selection.py:_select_unlocked
 * available/pending come from the pool's _available_entries seam.
 * {"entry": triple|null, "pending": [ids], "log_no_entries": bool,
 *  "throttle_cleared": bool, "persisted": bool} */
json_t *w20_credential_af0c5f_select_unlocked(const json_t *available, const json_t *pending,
                                               const char *strategy, const char *endpoint_url,
                                               int count, int rng_index);

/* PoP: w20_credential_af0c5f_peek @ agent/credential_pool_selection.py:peek
 * The current entry if it is still available (and matches `model`/`endpoint`),
 * else the first candidate with the current id, else the first candidate. */
json_t *w20_credential_af0c5f_peek(const json_t *current, const json_t *available,
                                   const json_t *model, const char *endpoint_url);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_CREDENTIAL_AF0C5F_H */
