#ifndef NOC_W20_WORKERLIFE_DACC9A_H
#define NOC_W20_WORKERLIFE_DACC9A_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/_store/worker_lifecycle.py
 * (gate w20_workerlife_dacc9a).
 *
 * The open connection, the holder and the store helpers are seams: the port
 * keeps the transaction body's decisions -- the tombstone guard, the profile
 * default, the _delegate_from strip and the exact 19-value INSERT parameter
 * tuple plus the steps that follow it.
 */

/* PoP: w20_workerlife_dacc9a_insert @ persistence/sqlite/conversation/_store/worker_lifecycle.py:insert_session_row_in_transaction
 * spec = {"session_id","source","model","model_config","system_prompt","user_id",
 *         "session_key","chat_id","chat_type","thread_id","parent_session_id",
 *         "cwd","profile_name","git_repo_root","origin_json","display_name",
 *         "transport_profile","row_exists","tombstone_exists",
 *         "own_profile_name","prompt_hash","now","retired_prefix"}
 * Answer: {"status":"ok","value":{"params":[19],"steps":[...],"profile_name":s,
 *          "model_config":v,"tombstone_queries":n}}
 *       | {"status":"raised","type":"AuthorityRefusal","message":"not_found"}.
 * params[9] is Python's json.dumps(model_config) (default separators) or null;
 * params[18] is the injected time.time(). */
json_t *w20_workerlife_dacc9a_insert(const json_t *spec);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_WORKERLIFE_DACC9A_H */
