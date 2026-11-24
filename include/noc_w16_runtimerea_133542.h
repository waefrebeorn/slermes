#ifndef NOC_W16_RUNTIMEREA_133542_H
#define NOC_W16_RUNTIMEREA_133542_H

#include "hermes_json.h"

/* C port of selected helpers in hermes_cli/runtime_readiness.py.
 * Returned json_t* / char* values are caller-owned. */

/* missing_credentials_suffix(provider): the leading-space hint suffix, "".
 * hint_kind: 0 = string hint, 1 = hook raises, 2 = non-string answer. */
char *w16_runtimerea_133542_missing_suffix(const char *provider, int profile_present,
                                           const char *hint, int hint_kind);

/* runtime_has_credentials(runtime): api_key callable / command / placeholder /
 * usable-secret verdict.  usable_secrets is a JSON array of strings. */
int w16_runtimerea_133542_has_credentials(const char *api_key, int api_key_callable,
                                          const char *command, json_t *usable_secrets);

/* readiness_failure(exc): {"ok": false, "error": str, "error_code": ...?}. */
json_t *w16_runtimerea_133542_readiness_failure(const char *error_text,
                                                int has_error_code, const char *error_code);

#endif
