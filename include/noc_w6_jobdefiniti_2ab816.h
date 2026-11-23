#ifndef NOC_W6_JOBDEFINITI_2AB816_H
#define NOC_W6_JOBDEFINITI_2AB816_H
#include "hermes_json.h"

/* C home for upstream Hermes cron/job_definition.py (gate w6_jobdefiniti_2ab816).
 * import_job_definitions is I/O (store lock + load/save); the authored-merge
 * policy itself is pure and is ported here. */

/* PoP: w6_jobdefiniti_merge_job_definition @ cron/job_definition.py:merge_job_definition
 * Returns a new object: scheduler-owned keys come from `local`, authored keys
 * overwrite the definition fields, `repeat` is rebuilt, and a schedule/zone
 * change drops pending_slot and re-arms the next run. Caller frees the result. */
json_t *w6_jobdefiniti_merge_job_definition(
    const json_t *local, const json_t *authored);

#endif /* NOC_W6_JOBDEFINITI_2AB816_H */
