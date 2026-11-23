#ifndef NOC_W12_DELEGATETO_66D55C_H
#define NOC_W12_DELEGATETO_66D55C_H

#include "hermes_json.h"

/* PoP port of tools/delegate_tool_tasks.py (gate w12_delegateto_66d55c). */

/* _normalize_task_images(task, i) -> [cleaned|null, error|null]. */
json_t *w12_delegateto_66d55c_normalize_task_images(json_t *task, long long i);
/* _coerce_task_images(task_list, images) -> [list, error|null]. */
json_t *w12_delegateto_66d55c_coerce_task_images(json_t *task_list, json_t *images);
/* _validate_batch_tasks(task_list) -> error string or null. */
json_t *w12_delegateto_66d55c_validate_batch_tasks(json_t *task_list);

#endif /* NOC_W12_DELEGATETO_66D55C_H */
