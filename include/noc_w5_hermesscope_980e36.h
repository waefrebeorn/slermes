#ifndef NOC_W5_HERMESSCOPE_980E36_H
#define NOC_W5_HERMESSCOPE_980E36_H

#include "hermes_json.h"

/* C port of the pure value-object validation and scope derivation of
 * hermes_scope.py. All exports are hermesscope_-prefixed.
 * Validation helpers return 1 (valid) / 0 (invalid). */

int hermesscope_principal_ok(json_t *v);   /* Principal.__post_init__ */
int hermesscope_system_ok(json_t *v);      /* System.__post_init__ */
int hermesscope_group_ok(json_t *v);       /* Group.__post_init__ */
int hermesscope_fence_ok(json_t *epoch, json_t *generation); /* Fence/_positive_int */
int hermesscope_is_local(const char *tenant); /* Scope.is_local */

/* Scope.home(): malloc'd agent path for a local scope, NULL (Python None) for
 * a hosted scope. Caller frees the returned string. */
char *hermesscope_home(const char *tenant, const char *agent);

#endif /* NOC_W5_HERMESSCOPE_980E36_H */
