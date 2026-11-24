#ifndef NOC_W17_SCHEDULERW_4C800D_H
#define NOC_W17_SCHEDULERW_4C800D_H

#include "hermes_json.h"

/* Python str(returncode) coercion is applied to rc (null -> "None"). */
char *w17_schedulerw_4c800d_external_worker_exited_reason(json_t *rc);

/* exec_unknown / owns_claim are the injected collaborator decisions.
 * Returns {"result": bool, "fence_owner": str|null}. */
json_t *w17_schedulerw_4c800d_record_outcome(json_t *job, int exec_unknown, int owns_claim);

#endif
