#ifndef NOC_W9_REDACTSPACE_453FC1_H
#define NOC_W9_REDACTSPACE_453FC1_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* PoP: w9_redactspace_453fc1_key_names_secret @ agent/redact_spaced_cfg.py:_key_names_secret
 * Keyword-placement test for a bare (dotted=false) or dotted (dotted=true) key. */
bool w9_redactspace_453fc1_key_names_secret(const char *name, bool dotted);

/* PoP: w9_redactspace_453fc1_is_code_expression @ agent/redact_spaced_cfg.py:_is_code_expression
 * True when an unquoted value looks like code (not a config secret). */
bool w9_redactspace_453fc1_is_code_expression(const char *key, const char *value);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_REDACTSPACE_453FC1_H */
