#ifndef NOC_W18_REDACTCRED_108D32_H
#define NOC_W18_REDACTCRED_108D32_H

#include "hermes_json.h"

/* C home for upstream Hermes agent/redact_credential_stores.py (regex-grammar helpers).
 * outside_spans' fn is a controlled seam, instantiated here as ASCII upper(). */

/* -> [[start,end], ...] byte spans of credential_process keys below an INI header */
json_t *w18_redactcred_108d32_aws_keys(const char *text);
/* -> text with fn applied outside the given [[start,end], ...] spans */
json_t *w18_redactcred_108d32_outside(const char *text, const json_t *spans);
json_t *w18_redactcred_108d32_constants(void);

#endif /* NOC_W18_REDACTCRED_108D32_H */
