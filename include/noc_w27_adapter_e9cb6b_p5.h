/*
 * noc_w27_adapter_e9cb6b_p5.h - C surface for the p5 slice of upstream
 * plugins/platforms/matrix/adapter.py (gate w27_adapter_e9cb6b_p5).
 *
 * The HTML rendering core: html.escape with quote=True, and the regex
 * Markdown->HTML fallback used when the ``markdown`` package is absent
 * (fenced/inline code, links, escaping, hr/headers/blockquotes/lists, inline
 * emphasis, newline-><br> with block adjacency fixups).  The sanitizer module
 * (adapter_html) and the i18n/templates stay seamed.
 */
#ifndef NOC_W27_ADAPTER_E9CB6B_P5_H
#define NOC_W27_ADAPTER_E9CB6B_P5_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* html.escape(text, quote=True): & < > " ' */
json_t *w27_adapter_html_escape(const json_t *text);

/* Markdown -> org.matrix.custom.html via the regex fallback. */
json_t *w27_adapter_markdown_to_html_fallback(const json_t *text);

#ifdef __cplusplus
}
#endif
#endif
