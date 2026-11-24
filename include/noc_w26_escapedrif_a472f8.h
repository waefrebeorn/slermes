/*
 * noc_w26_escapedrif_a472f8.h - C surface for upstream
 * tools/escape_drift_autocorrect.py (gate w26_escapedrif_a472f8).
 *
 * Conservative correction of escape drift: every decision below is a pure
 * function of the three strings (old_string, new_string, file content).  The
 * patch tool owns the file; this file owns only what the values decide.
 */
#ifndef NOC_W26_ESCAPEDRIF_A472F8_H
#define NOC_W26_ESCAPEDRIF_A472F8_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* tools/escape_drift_autocorrect.py:AmbiguousEscapeDriftError */
#define W26_ESCAPEDRIF_A472F8_ERROR "AmbiguousEscapeDriftError"

/* _halve_backslash_runs: every maximal backslash run -> half its length. */
json_t *w26_escapedrif_a472f8_halve(const char *s);

/* _all_backslash_runs_even. */
json_t *w26_escapedrif_a472f8_runs_even(const char *s);

/* _strip_spurious_quote_escapes: ``\'`` -> ``'``, ``\"`` -> ``"``. */
json_t *w26_escapedrif_a472f8_strip_quotes(const char *s);

/* _quote_escapes_all_spurious over an array of strings. */
json_t *w26_escapedrif_a472f8_quotes_all_spurious(const json_t *strings);

/* maybe_correct_backslash_doubling -> {"old","new","error"}. */
json_t *w26_escapedrif_a472f8_backslash_correct(const char *old_string,
                                               const char *new_string,
                                               const char *content);

/* maybe_correct_quote_escaping -> {"old","new","error"}. */
json_t *w26_escapedrif_a472f8_quote_correct(const char *old_string,
                                           const char *new_string,
                                           const char *content);

/* maybe_correct_escape_drift -> {"old","new","note","error"}. */
json_t *w26_escapedrif_a472f8_escape_drift(const char *old_string,
                                          const char *new_string,
                                          const char *content);

/* The note/error strings the module reports, as an object. */
json_t *w26_escapedrif_a472f8_notes(void);

#ifdef __cplusplus
}
#endif
#endif
