/*
 * noc_w26_richcodere_c9c20f.h - C surface for upstream
 * plugins/platforms/telegram/rich_code_regions.py (gate w26_richcodere_c9c20f).
 *
 * The code spans of a rich-message markdown text, found in one linear pass:
 * fenced blocks (``` / ~~~, closed, streamed-open, or behind a lead-in), the
 * indented blocks, the inline backtick spans and -- optionally -- the
 * <pre>/<code> elements.  Everything is a pure function of the text; spans are
 * half-open byte offsets (the upstream uses character offsets, so inputs are
 * ASCII by contract).
 */
#ifndef NOC_W26_RICHCODERE_C9C20F_H
#define NOC_W26_RICHCODERE_C9C20F_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* text, html -> [[start, end], ...] sorted and disjoint. */
json_t *w26_richcodere_c9c20f_regions(const json_t *text, const json_t *html);

/* text -> text with its Markdown code removed (<pre>/<code> kept). */
json_t *w26_richcodere_c9c20f_strip_code(const json_t *text);

/* text, transform, html -> text with transform applied to each gap between
 * code regions.  transform is one of "identity", "upper", "lower", "bracket",
 * "drop". */
json_t *w26_richcodere_c9c20f_map_outside(const json_t *text, const json_t *transform,
                                          const json_t *html);

/* The fence widths the scanner is tuned with, as an object. */
json_t *w26_richcodere_c9c20f_constants(void);

#ifdef __cplusplus
}
#endif
#endif
