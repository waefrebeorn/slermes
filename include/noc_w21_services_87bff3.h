#ifndef NOC_W21_SERVICES_87BFF3_H
#define NOC_W21_SERVICES_87BFF3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/services.py (gate w21_services_87bff3).
 *
 * Both exports answer without a healthy store: the corruption latch and the
 * readiness probe.  The probe's decision tree is pure; what stays upstream is
 * `Path.exists()`, the `mode=ro` connection (`connect_tracked` +
 * `read_only_db_uri`), `PRAGMA query_only` / the `sqlite_master` read, and
 * `note_storage_error`'s classification of the exception (the latch is a DB
 * seam, so the caller reports whether it latched).
 */

#define W21_SERVICES_87BFF3_STORAGE_OK "ok"
#define W21_SERVICES_87BFF3_STORAGE_CORRUPT "corrupt"

/* PoP: w21_services_87bff3_probe @ persistence/sqlite/services.py:probe_state_db
 * `req` is the store's situation:
 *   {"exists": bool, "state": "ok"|"corrupt", "connect": "ok"|"raise",
 *    "exc_type": <class name of the failed probe>, "note": "latched"|"none"}
 * Answer is the `StorageProbe` payload {"status":"ok"|"degraded",
 * "detail":null|"not initialized"|"corrupt"|<exc class name>}, in this order:
 *   - no file            -> ok / "not initialized"
 *   - the latch is latched -> degraded / "corrupt" (no connection is opened)
 *   - the read-only probe succeeds -> ok / null
 *   - it raises and the latch takes it -> degraded / "corrupt"
 *   - it raises and the latch declines -> degraded / the exception's class name
 */
json_t *w21_services_87bff3_probe(const json_t *req);

/* PoP: w21_services_87bff3_health_state @ persistence/sqlite/services.py:SQLiteStorageHealth.storage_state
 * `StorageState.CORRUPT if storage_state(self._path) == STORAGE_CORRUPT else
 * StorageState.OK` -> the enum member's name, "CORRUPT" or "OK". */
const char *w21_services_87bff3_health_state(const char *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_SERVICES_87BFF3_H */
