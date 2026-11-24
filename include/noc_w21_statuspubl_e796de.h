#ifndef NOC_W21_STATUSPUBL_E796DE_H
#define NOC_W21_STATUSPUBL_E796DE_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/web_routers/status_public.py (gate w21_statuspubl_e796de).
 *
 * `/api/status` is public, so every field an unverified caller may read is an
 * allowlisted, value-validated projection. The port keeps the whole projection
 * (including the normalize_updated_at / classify_exit_reason helpers the
 * module delegates to) and returns the resulting body. */

/* PoP: w21_statuspubl_e796de_anonymous_entry @ hermes_cli/web_routers/status_public.py:_anonymous_platform_entry
 * One platform entry reduced to state / error_code / needs_attention /
 * retrying_since / updated_at. A non-dict value projects to {}. */
json_t *w21_statuspubl_e796de_anonymous_entry(const json_t *value);

/* PoP: w21_statuspubl_e796de_redact @ hermes_cli/web_routers/status_public.py:redact_status_for_unverified
 * `redact_status_for_unverified(status)` as a value: every platform entry
 * projected through the allowlist, `gateway_exit_reason` reduced to a bounded
 * class unless it already is a source-controlled code, and
 * `multiplex_standalone_reason` cleared. The caller's object is not mutated. */
json_t *w21_statuspubl_e796de_redact(const json_t *status);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_STATUSPUBL_E796DE_H */
