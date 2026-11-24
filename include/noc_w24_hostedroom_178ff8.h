/*
 * noc_w24_hostedroom_178ff8.h - C surface for upstream
 * persistence/sqlite/gateway/_hosted_rooms_common.py (gate w24_hostedroom_178ff8).
 *
 * The four transaction helpers every hosted-room repository shares, ported as
 * decisions over values: the compare-and-swap rowcount verdict, the epoch fence
 * test (including the ``type(epoch) is not int`` rule), the sqlite_master
 * existence probe and the PRAGMA table_info column set.  The statements
 * themselves - the UPDATE, the catalog SELECT, the PRAGMA - stay with the
 * caller: every function here takes the row (or the rowcount) it read.
 */
#ifndef W24_HOSTEDROOM_178FF8_H
#define W24_HOSTEDROOM_178FF8_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"rowcount": <value>} -> {"ok": <rowcount == 1>}
 * fenced_update raises the caller's error for anything but exactly one row. */
json_t *w24_hostedroom_178ff8_fenced_update(const json_t *req);

/* {"epoch": <value>, "epoch_type": "int"|"float"|"bool"|"str"|"null",
 *  "row_present": <bool>, "row_epoch": <value>} ->
 * {"ok": true} or {"ok": false, "reason", "entity", "entity_id"} (FenceStale). */
json_t *w24_hostedroom_178ff8_epoch(const json_t *req);

/* {"row_present": <bool>} -> the sqlite_master row is not None. */
json_t *w24_hostedroom_178ff8_table_exists(const json_t *req);

/* {"rows": [[<cid>, <name>, ...], ...]} -> frozenset(row[1]) as a sorted list.
 * A row without a name column is the Python IndexError, answered as
 * {"error": "index"}. */
json_t *w24_hostedroom_178ff8_table_columns(const json_t *req);

/* StaleFenceKind values and the epoch fence's reason, as an object. */
json_t *w24_hostedroom_178ff8_constants(void);

#ifdef __cplusplus
}
#endif
#endif
