#ifndef SLERMES_BROWSER_SUPERVISOR_REDACT_H
#define SLERMES_BROWSER_SUPERVISOR_REDACT_H

#include <stdbool.h>

/*
 * browser_supervisor_redact — redaction helpers extracted from
 * port_browser_supervisor.c (v555 monolith split). These delegate to the
 * faithful agent/redact port in browser_redact.{h,c}.
 *
 * Public API:
 *   browser_supervisor_redact_cdp_error_text  @ tools/browser_supervisor.py:_redact_cdp_error_text
 *   browser_supervisor_redact_supervisor_text  @ tools/browser_supervisor.py:_redact_supervisor_text
 */

/* PoP: browser_supervisor_redact_cdp_error_text @ tools/browser_supervisor.py:_redact_cdp_error_text */
/* Redact CDP endpoint credentials from an error's string form. Returns a */
/* newly-allocated string (caller frees); "<error redacted>" on NULL input. */
char *browser_supervisor_redact_cdp_error_text(const char *error_text);

/* PoP: browser_supervisor_redact_supervisor_text @ tools/browser_supervisor.py:_redact_supervisor_text */
/* Redact page-originated text before exposing supervisor snapshots. Returns a */
/* newly-allocated string (caller frees); "" on NULL input. */
char *browser_supervisor_redact_supervisor_text(const char *value);

#endif /* SLERMES_BROWSER_SUPERVISOR_REDACT_H */
