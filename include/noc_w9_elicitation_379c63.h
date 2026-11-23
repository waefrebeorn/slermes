#ifndef NOC_W9_ELICITATION_379C63_H
#define NOC_W9_ELICITATION_379C63_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure elicitation helpers of acp_adapter/elicitation.py.
 * Every symbol is prefixed with the gate id. */

/* PoP: w9_elicitation_379c63_client_supports_form @ acp_adapter/elicitation.py:client_supports_form_elicitation */
int w9_elicitation_379c63_client_supports_form(const json_t *params);

/* PoP: w9_elicitation_379c63_build_clarify_elicitation @ acp_adapter/elicitation.py:build_clarify_elicitation
 * Returns the requested form envelope as a JSON object. Caller frees. */
json_t *w9_elicitation_379c63_build_clarify_elicitation(const char *session_id, const json_t *prompt);

/* PoP: w9_elicitation_379c63_elicited_answer @ acp_adapter/elicitation.py:elicited_answer
 * Returns a JSON string or JSON null. Caller frees. */
json_t *w9_elicitation_379c63_elicited_answer(const json_t *response);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_ELICITATION_379C63_H */
