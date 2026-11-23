#ifndef NOC_SRACT_H
#define NOC_SRACT_H
/* C port of session_rules/activity.py (gate: sract). */
#include <stddef.h>

long long sract_activity_description_max(void);              /* 120 */
char *sract_bound_activity_description(const char *description); /* malloc'd; NULL == None */
const char *sract_normalize_activity_provenance(const char *provenance); /* static canonical */

#endif /* NOC_SRACT_H */
