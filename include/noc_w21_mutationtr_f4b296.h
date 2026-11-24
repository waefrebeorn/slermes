#ifndef NOC_W21_MUTATIONTR_F4B296_H
#define NOC_W21_MUTATIONTR_F4B296_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/_store/mutation_transcript.py
 * (gate w21_mutationtr_f4b296): `rewind_in_transaction`, the rewind body run
 * inside a write transaction.
 *
 * The port owns exactly what that module owns: the call order, the CAS on
 * `expected_active_ids`, the target row / role asserts with their exact
 * messages, the destructive `IN (...)` list, the replacement insert and
 * `last_insert_rowid`, the rewind-count bump, the counter rewrite and the head
 * id -- every statement and every bound parameter, in order.
 *
 * Seams, named (each belongs to another module or to the database):
 *  * `conn`             -- the sqlite3 connection; the request's `db` answers
 *                          its four row-returning statements, and the port
 *                          emits the statement log for comparison.
 *  * `_check_transcript_write_guards`  (messages.py) -- `guards`
 *  * `_split_rewind_target`           (messages.py) -- `split`
 *  * `_insert_message_rows`           (messages.py) -- recorded as `inserts`
 *  * `_active_transcript_counts`      (messages.py) -- `counts`
 *
 * Request:
 *   session_id, target_message_id, preserve_compaction_handoff,
 *   expected_active_ids[]|null, expected_target_content|absent,
 *   guards{action:"pass"|"raise",type,message},
 *   split{action:"none"|"return"|"raise",value,type,message},
 *   counts[message_count,tool_call_count],
 *   db{active_ids[],row|null,rewound_ids[],last_rowid,head_id}
 *
 * Response:
 *   {"status":"ok","target_row":{...},"ids":[...],"head_id":N|null,
 *    "replacement_message_id":N|null,"guard_args":{...},
 *    "split_args":{...}|null,"inserts":[...]|null,
 *    "statements":[{"sql":"...","params":[...]},...]}
 *   {"status":"raised","type":"...","message":"..."}
 */
json_t *w21_mutationtr_f4b296_rewind(const json_t *req);

/* PoP: w21_mutationtr_f4b296_placeholders @ persistence/sqlite/conversation/_store/sql_common.py:_placeholders
 * `",".join("?" for _ in items)`. Caller frees. */
char *w21_mutationtr_f4b296_placeholders(double count);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_MUTATIONTR_F4B296_H */
