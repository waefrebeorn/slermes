#ifndef NOC_W18_DELEGATETO_D76C3E_H
#define NOC_W18_DELEGATETO_D76C3E_H

#include "hermes_json.h"

/* C home for upstream Hermes tools/delegate_tool_resumable.py.
 * Pure decision / identity helpers, ported verbatim:
 *   classify()          -> [state, resume_mode, next_attempt_at]
 *   endpoint_identity() -> [provider, sha256(base_url) | null]
 * `now` is the injected time.time() value (seam in the oracle). */
json_t *w18_delegateto_d76c3e_classify(const json_t *entry, const char *stop_cause, double now);
json_t *w18_delegateto_d76c3e_endpoint_identity(const json_t *provider, const char *base_url_or_null);

#endif /* NOC_W18_DELEGATETO_D76C3E_H */
