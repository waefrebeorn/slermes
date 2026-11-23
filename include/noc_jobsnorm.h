#ifndef NOC_JOBSNORM_H
#define NOC_JOBSNORM_H
#include <stdbool.h>
#include "hermes_json.h"

/* C port of cron/jobs_normalize.py. Every exported symbol is jobsnorm_-prefixed.
 * A NULL `value` models a Python value that is not a str (isinstance check failed).
 * Malloc'd char* returns are caller-owned; json_t* returns (NULL == Python None)
 * are caller-owned via json_free. */
char *jobsnorm_normalize_optional_text(const char *value, bool strip_trailing_slash);
char *jobsnorm_normalize_base_url(const char *value);
json_t *jobsnorm_normalize_str_list(const json_t *items);
json_t *jobsnorm_normalize_context_from(const json_t *value);
char *jobsnorm_normalize_failure_deliver(const json_t *value);
#endif
