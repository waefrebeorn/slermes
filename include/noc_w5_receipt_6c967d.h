#ifndef NOC_W5_RECEIPT_6C967D_H
#define NOC_W5_RECEIPT_6C967D_H
#include <stdbool.h>
#include "hermes_json.h"

/* C home for upstream Hermes pm/receipt.py (gate w5_receipt_6c967d).
 * The in-flight / finalized receipt state machine is ported as pure data:
 * timestamps are the fixed marker "T" (the oracle freezes _utc_now_iso), and
 * file rotation / ContextVars stay in Python. update_id NULL means Python
 * None; ops is a JSON array of record_* operations. */

/* PoP: w5_receipt_6c967d_receipt @ pm/receipt.py:begin + record_* + snapshot */
json_t *w5_receipt_6c967d_receipt(const char *kind, const char *update_id,
                                  json_t *ops);

/* PoP: w5_receipt_6c967d_finalized @ pm/receipt.py:finalize + last_completed */
json_t *w5_receipt_6c967d_finalized(const char *kind, const char *update_id,
                                    const char *outcome, int exit_code,
                                    json_t *ops);

#endif /* NOC_W5_RECEIPT_6C967D_H */
