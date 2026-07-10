/*
 * browser_supervisor_redact.c — redaction helpers extracted from
 * port_browser_supervisor.c (v555 monolith split). Delegates to the faithful
 * agent/redact port in browser_redact.{h,c}.
 *
 * Python's _redact_cdp_error_text / _redact_supervisor_text route through
 * agent.redact.redact_cdp_url / redact_sensitive_text (force=True). The C
 * previously did crude string masking; this delegates to the real redactor.
 */

#include "browser_supervisor_redact.h"
#include "browser_redact.h"
#include <stdlib.h>
#include <string.h>

/* PoP: browser_supervisor_redact_cdp_error_text @ tools/browser_supervisor.py:_redact_cdp_error_text */
char *browser_supervisor_redact_cdp_error_text(const char *error_text)
{
    if (!error_text) return strdup("<error redacted>");
    char *r = browser_redact_cdp_url(error_text);
    return r ? r : strdup("<error redacted>");
}

/* PoP: browser_supervisor_redact_supervisor_text @ tools/browser_supervisor.py:_redact_supervisor_text */
char *browser_supervisor_redact_supervisor_text(const char *value)
{
    if (!value) return strdup("");
    char *r = browser_redact_sensitive_text(value);
    return r ? r : strdup("");
}
