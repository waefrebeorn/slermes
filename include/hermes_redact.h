/*
 * hermes_redact.h — Public API for the secret/token redaction engine.
 *
 * Faithful extraction from the monolithic hermes.h god header (the
 * god-header-elimination pass). The redaction engine lives in
 * src/agent/redact.c. Translation units that only need redaction no
 * longer drag in the entire master header.
 */

#ifndef HERMES_REDACT_H
#define HERMES_REDACT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Redact secrets/tokens/keys from arbitrary text. Caller frees result. */
char *hermes_redact(const char *input);
/* Like hermes_redact but preserves key:value patterns inside source code. */
char *hermes_redact_code_file(const char *input);
/* Add a custom regex/string redaction pattern at runtime. */
bool hermes_redact_add_pattern(const char *pattern);
/* Clear all runtime-added patterns. */
void hermes_redact_clear_patterns(void);
/* Load patterns from a comma/whitespace-separated config string. */
void hermes_redact_load_config(const char *patterns_str);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_REDACT_H */
