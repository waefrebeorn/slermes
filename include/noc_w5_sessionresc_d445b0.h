#ifndef NOC_W5_SESSIONRESC_D445B0_H
#define NOC_W5_SESSIONRESC_D445B0_H

#include "hermes_json.h"

/* C port of session_rescue.py's transcript-diversion plan. The live function
 * appends JSONL records to <sessions_dir>/<sid>.jsonl; this leaf computes
 * exactly what would be appended (and whether anything is written at all),
 * which is the filesystem-independent observable of the function.
 * All exports are sessionresc_-prefixed. */

/* Returns JSON null when nothing would be written (empty/whitespace id or
 * empty messages); otherwise {"name": "<sid>.jsonl", "records": [...]}.
 * Caller owns the returned json_t. */
json_t *sessionresc_divert_plan(const char *session_id, json_t *messages);

#endif /* NOC_W5_SESSIONRESC_D445B0_H */
