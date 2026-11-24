#ifndef NOC_W22_MODELCONFI_0C8013_H
#define NOC_W22_MODELCONFI_0C8013_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/_store/model_config.py (gate
 * w22_modelconfi_0c8013).
 *
 * `update_conversation_model_config_value` runs its read/apply/write inside a
 * write transaction; the sqlite handle, the compression-tip walk and the
 * `update` callable are the DB/IO seam. What the transaction *decides* is pure:
 *
 *   - `_parse_model_config`: a JSON TEXT column (or a dict, or anything else)
 *     decoded to a dict copy -- tolerant: bad JSON and non-dicts become {}.
 *   - the value application: `config[key] = update(config.get(key))`, with a
 *     None result removing the key.
 *   - `_sql_json_extract`: the non-throwing JSON marker SQL expression, and
 *     the ordered id query built on it.
 */

/* PoP: w22_modelconfi_0c8013_update_config @ persistence/sqlite/conversation/_store/model_config.py:update_conversation_model_config_value
 * The post-transaction `model_config` object: `_parse_model_config(row_config)`
 * with `config[key]` set to `new_value`, or the key removed when `new_value`
 * is NULL/absent (Python: update() returned None). Returns a fresh object. */
json_t *w22_modelconfi_0c8013_update_config(const char *row_config,
                                            const char *key,
                                            const json_t *new_value);

/* PoP: w22_modelconfi_0c8013_json_extract @ persistence/sqlite/_schema_ddl.py:_sql_json_extract
 * `_sql_json_extract(expression, path)` -- the CASE-wrapped json_extract text
 * with `path` quoted as a single SQL literal (embedded quotes doubled). */
char *w22_modelconfi_0c8013_json_extract(const char *expression, const char *path);

/* PoP: w22_modelconfi_0c8013_select_ids_sql @ persistence/sqlite/conversation/_store/model_config.py:session_ids_with_model_config_key
 * The exact SELECT whose rows that function maps to ids, newest first. */
char *w22_modelconfi_0c8013_select_ids_sql(const char *key);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_MODELCONFI_0C8013_H */
