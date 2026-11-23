#ifndef NOC_W13_CRONSTORE_1B947C_H
#define NOC_W13_CRONSTORE_1B947C_H

#include "hermes_json.h"

/* PoP port of gateway/cron_store_notices.py (gate w13_cronstore_1b947c). */

/* notice_facts(boot, degraded, events) -> list[str] action trace of the
 * installed transition listener (set/send/schedule/cancel decisions). */
json_t *w13_cronstore_1b947c_notice_facts(json_t *boot, json_t *degraded, json_t *events);

#endif /* NOC_W13_CRONSTORE_1B947C_H */
