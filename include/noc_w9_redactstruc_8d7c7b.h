#ifndef NOC_W9_REDACTSTRUC_8D7C7B_H
#define NOC_W9_REDACTSTRUC_8D7C7B_H

#include "hermes_json.h"

/* agent/redact_structured.py — fail-closed structure-preserving redaction.
 * The external redaction engine (agent.redact.redact_sensitive_text) is a seam:
 * both the C port and the oracle use the same deterministic stand-in that maps
 * the token "SECRET" to "***". */
json_t *w9_redactstruc_8d7c7b_redact_structured(json_t *payload);

#endif /* NOC_W9_REDACTSTRUC_8D7C7B_H */
