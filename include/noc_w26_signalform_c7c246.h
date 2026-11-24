/*
 * noc_w26_signalform_c7c246.h - C surface for upstream
 * plugins/platforms/signal/signal_format.py (gate w26_signalform_c7c246).
 *
 * Signal has no Markdown: bodies carry plain text plus ``bodyRanges``
 * (``start:length:STYLE`` in UTF-16 code units).  This is the pure string
 * machinery both the live adapter and the standalone send paths share — the
 * fence splitter, the bullet normaliser, the UTF-16 length ruler, the full
 * markdown -> (text, styles) pass — and the module's literal pattern tables.
 *
 * Seam: re-aligning a GFM table needs ``wcwidth`` column widths; a table
 * outside a fence is reported as {"error": "table_seam"} instead of silently
 * emitting unaligned columns.
 */
#ifndef NOC_W26_SIGNALFORM_C7C246_H
#define NOC_W26_SIGNALFORM_C7C246_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W26_SIGNALFORM_C7C246_TABLE_WIDTH 40

/* _utf16_len(s): length in UTF-16 code units. */
json_t *w26_signalform_c7c246_utf16_len(const json_t *text);

/* _split_fenced(source): [prose, fenced, prose, ...]. */
json_t *w26_signalform_c7c246_split_fenced(const json_t *source);

/* _normalize_bullet_markers(source): "- item" -> "• item" outside fences. */
json_t *w26_signalform_c7c246_normalize_bullets(const json_t *source);

/* markdown_to_signal(text): {"text": ..., "styles": [...]} (or the table seam). */
json_t *w26_signalform_c7c246_markdown_to_signal(const json_t *text);

/* The literal pattern tables and the style vocabulary. */
json_t *w26_signalform_c7c246_constants(void);

#ifdef __cplusplus
}
#endif
#endif
