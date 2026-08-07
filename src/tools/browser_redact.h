#ifndef SLERMES_BROWSER_REDACT_H
#define SLERMES_BROWSER_REDACT_H

#include <stdbool.h>

/*
 * browser_redact — faithful C port of agent/redact.py (redact_sensitive_text +
 * redact_cdp_url). Extracted as the redaction backend for the browser
 * supervisor split (v555). See browser_redact.c for the fidelity notes.
 *
 * Public API:
 *   browser_redact_sensitive_text  @ agent/redact.py:redact_sensitive_text
 *   browser_redact_cdp_url         @ agent/redact.py:redact_cdp_url
 */

/* PoP: browser_redact_sensitive_text @ agent/redact.py:redact_sensitive_text */
/* Returns a newly-allocated redacted string (caller frees). NULL if value is */
/* NULL; "" if value is empty. force=True path (always redact). */
char *browser_redact_sensitive_text(const char *value);

/* PoP: browser_redact_cdp_url @ agent/redact.py:redact_cdp_url */
/* redact_sensitive_text + URL query-param + user:pass@ userinfo redaction. */
char *browser_redact_cdp_url(const char *value);

#endif /* SLERMES_BROWSER_REDACT_H */
