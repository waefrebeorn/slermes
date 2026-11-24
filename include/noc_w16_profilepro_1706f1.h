#ifndef NOC_W16_PROFILEPRO_1706F1_H
#define NOC_W16_PROFILEPRO_1706F1_H

#include "hermes_json.h"

/* C port of selected pure helpers in hermes_cli/profile_provision.py.
 * Returned json_t* values are caller-owned (json_free).
 *
 * The redaction step in the Python source (redact_create_text ->
 * agent.redact.redact_sensitive_text) is implemented here as the identity
 * transform; that is exactly its behaviour for the non-secret text this
 * contract handles.  Fixture text must not contain secret/URL-credential
 * patterns. */

/* ProfileCreateRequest.fresh: no source profile => True. */
int w16_profilepro_1706f1_fresh(const char *clone_from, int clone_all, int clone_config);

/* refusal(exc): {"text": str, "type": str, "parts": [str, ...]} */
json_t *w16_profilepro_1706f1_refusal(const char *text, const char *exc_type);

/* message_from({"message_parts": [...]}): joined string parts, redacted. */
char *w16_profilepro_1706f1_message_from(json_t *parts);

/* refusal_from({"type": ..., "message_parts": [...]}):
 * {"type": str, "message": str} or NULL (JSON null) for an unknown type. */
json_t *w16_profilepro_1706f1_refusal_from(const char *exc_type, json_t *parts);

#endif
