#ifndef NOC_W12_APPROVALPR_BFD8EF_H
#define NOC_W12_APPROVALPR_BFD8EF_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_approvalpr_bfd8ef_preapproved_result @ tools/approval_provenance.py:preapproved_result
 * findings: JSON array of [pattern_key, description] pairs; scopes: object key->scope label.
 * Returns an owned json_t dict; caller frees. */
struct json_t *w12_approvalpr_bfd8ef_preapproved_result(const struct json_t *findings,
                                                        const struct json_t *scopes);

/* PoP: w12_approvalpr_bfd8ef_allowlisted_result @ tools/approval_provenance.py:allowlisted_result
 * Returns an owned json_t dict; caller frees. */
struct json_t *w12_approvalpr_bfd8ef_allowlisted_result(const char *rule, const char *description);

/* PoP: w12_approvalpr_bfd8ef_preapproval_note @ tools/approval_provenance.py:preapproval_note
 * entries: JSON array of {description, scope, rule?} dicts. Returns a malloc'd string. */
char *w12_approvalpr_bfd8ef_preapproval_note(const struct json_t *entries);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_APPROVALPR_BFD8EF_H */
