/*
 * noc_w24_foreignses_93c0f0.h - C surface for upstream
 * hermes_cli/foreign_sessions_browser.py (gate w24_foreignses_93c0f0).
 *
 * The desktop session importer's backend-local half. Scanning the disk, reading
 * and parsing a foreign log, and the persistence layer are seams; what is
 * ported here is everything the browser decides from values alone: the opaque
 * handle it hands the client, the page/handle validation that keeps a request
 * from naming a path, the display title, and the bounded preview.
 */
#ifndef W24_FOREIGNSES_93C0F0_H
#define W24_FOREIGNSES_93C0F0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {source, path} -> sha256("<source>:<path>") hex digest: the only
 * identifier a preview/import request may carry. */
json_t *w24_foreignses_93c0f0_handle_for(const json_t *req);

/* req: {handle, known: [handle...]} -> null when the handle is well-formed and
 * still listed, else the refusal text resolve_foreign_session() raises. */
json_t *w24_foreignses_93c0f0_resolve_error(const json_t *req);

/* req: {offset, limit} -> null when the page is valid, else "Invalid session
 * page" (offset >= 0 int, 1 <= limit <= 50 int). */
json_t *w24_foreignses_93c0f0_validate_page(const json_t *req);

/* req: {parsed: {turns: [{role, content}...], title_guess}, source} -> the
 * browser display title. */
json_t *w24_foreignses_93c0f0_display_title(const json_t *req);

/* req: {parsed: {turns: [...]}} -> {messages, total, truncated}: the last 40
 * turns with content clipped to 8000 characters. */
json_t *w24_foreignses_93c0f0_preview(const json_t *req);

/* req: {size, turn_count} -> null when the log is parseable, else the refusal
 * text _parse() raises. */
json_t *w24_foreignses_93c0f0_parse_error(const json_t *req);

/* req: {offset, limit, total} -> next page offset, or null on the last page. */
json_t *w24_foreignses_93c0f0_next_offset(const json_t *req);

/* MAX_LOG_BYTES, the preview bounds and the source label table. */
json_t *w24_foreignses_93c0f0_constants(void);

#ifdef __cplusplus
}
#endif
#endif
