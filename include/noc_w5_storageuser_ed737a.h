#ifndef NOC_W5_STORAGEUSER_ED737A_H
#define NOC_W5_STORAGEUSER_ED737A_H

#include "hermes_json.h"

/* C port of the pure copy/formatting leaves of storage_user_copy.py.
 * All exports are storageuser_-prefixed. Returned values are caller-owned. */

/* storage_failure_details(text, limit): whitespace-normalised raw cause,
 * truncated to limit with a "..." suffix. */
char *storageuser_failure_details(const char *text, long long limit);

/* describe_storage_failure with the three external facts injected explicitly
 * (classify result, is-disk-full result, profile CLI selector): the seams
 * replace process/config state that a JSON harness cannot mutate.
 * Returns {"cause":..., "code":..., "gloss":..., "action":...}. */
json_t *storageuser_describe_ex(const char *cause, int disk_full, const char *profile_arg);

/* STORAGE_RECOVERY_DOCS_URL. */
const char *storageuser_recovery_docs_url(void);

#endif /* NOC_W5_STORAGEUSER_ED737A_H */
