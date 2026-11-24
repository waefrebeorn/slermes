/*
 * noc_w25_reporter_f01f70.h - C surface for upstream agent/lsp/reporter.py
 * (gate w25_reporter_f01f70).
 *
 * The formatting core of the LSP diagnostic reporter: field sanitising, the
 * one-line diagnostic rendering, the severity-filtered <diagnostics> block and
 * the hard-capped truncation.  Every seam (the language server, the file read,
 * the tool result it is spliced into) stays in the caller; what is ported is
 * the pure string transformation those values decide.
 */
#ifndef NOC_W25_REPORTER_F01F70_H
#define NOC_W25_REPORTER_F01F70_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module-level literals (agent/lsp/reporter.py). */
#define W25_REPORTER_F01F70_MAX_PER_FILE 20
#define W25_REPORTER_F01F70_MAX_TOTAL_CHARS 4000
#define W25_REPORTER_F01F70_MAX_MESSAGE_CHARS 300
#define W25_REPORTER_F01F70_MAX_CODE_CHARS 80
#define W25_REPORTER_F01F70_MAX_SOURCE_CHARS 80

/* {"value": <any>, "limit": <int>} -> the sanitised field string. */
json_t *w25_reporter_f01f70_sanitize(const json_t *req);

/* <diagnostic dict> -> the one-line "SEV [line:col] msg [code] (source)". */
json_t *w25_reporter_f01f70_format_diagnostic(const json_t *d);

/* {"file_path","diagnostics","severities","max_per_file"} ->
 * the "<diagnostics file=...>" block, or "" when nothing passes the filter. */
json_t *w25_reporter_f01f70_report_for_file(const json_t *req);

/* {"s": <str>, "limit": <int>} -> elide(s, limit). */
json_t *w25_reporter_f01f70_truncate(const json_t *req);

/* The module constants, as an object. */
json_t *w25_reporter_f01f70_constants(void);

#ifdef __cplusplus
}
#endif
#endif
