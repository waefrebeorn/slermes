#ifndef NOC_W9_EXECUTIONID_8A3A66_H
#define NOC_W9_EXECUTIONID_8A3A66_H

#include "hermes_json.h"

/* cron/execution_identity.py -> enter_cron_execution()/current_cron_execution().
 * The profile argument is the resolved value of current_profile_name("default"). */
json_t *w9_executionid_8a3a66_enter_cron_execution_ex(json_t *job, const char *execution_id,
                                                      json_t *record, const char *profile);
json_t *w9_executionid_8a3a66_current_cron_execution_ex(json_t *bound, int delegated);

#endif /* NOC_W9_EXECUTIONID_8A3A66_H */
