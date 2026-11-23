#ifndef NOC_W6_PLUGINEVICT_1966C8_H
#define NOC_W6_PLUGINEVICT_1966C8_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: pluginevict_discard @ pm/plugin_eviction.py:_discard
 * Returns the input "names" list after removing entries found in "spellings"
 * (Python's in-place _discard, order preserved). Caller frees. */
json_t *pluginevict_discard(const json_t *names, const json_t *spellings);

/* PoP: pluginevict_list @ pm/plugin_eviction.py:_list
 * Returns [list_value, mutated_plugins] so both the returned list and the
 * get-or-create mutation of plugins[key] are observable. Caller frees. */
json_t *pluginevict_list(const json_t *plugins, const char *key);

/* PoP: pluginevict_prune @ pm/plugin_eviction.py:_prune_record
 * Returns the plugins mapping after pruning disabled_by_update records.
 * Caller frees. */
json_t *pluginevict_prune(const json_t *plugins);

/* PoP: pluginevict_settle @ pm/plugin_eviction.py:settle_record
 * Returns the plugins mapping after settled names are forgotten and records
 * pruned. Caller frees. */
json_t *pluginevict_settle(const json_t *plugins, const json_t *settled);

/* PoP: pluginevict_notices @ pm/plugin_eviction.py:_outcome_notices
 * `facts` supplies entries [[plugins_dir, name, plugin_dir] ...], recorded
 * [{"entry":[...],"record":{...}} ...], reasons/waiting maps keyed by plugin
 * dir, and joined entries. Returns the list of human-readable notices.
 * Caller frees. */
json_t *pluginevict_notices(const json_t *facts);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_PLUGINEVICT_1966C8_H */
