/*
 * noc_w20_adapterhtm_fd926b.h -- public C API for the port of
 *   plugins/platforms/matrix/adapter_html.py (wave 20 batch 9).
 *
 * Matrix outbound HTML: the $...$/$$...$$ sentinel tokenizer, the
 * data-mx-maths expansion, the allowlist sanitizer and the pre-sanitize pass
 * that strips unsafe raw HTML before Markdown conversion.
 *
 * Buffers are heap strings owned by the caller (free with free()); a NULL
 * result means allocation failure. `store` is the ordered token store as a
 * JSON array of {"tag": "div"|"span", "tex": "..."} objects.
 */
#ifndef NOC_W20_ADAPTERHTM_FD926B_H
#define NOC_W20_ADAPTERHTM_FD926B_H

#include "hermes_json.h"

/* Display ($$...$$) and inline ($...$) TeX become sentinel tokens.
 * Returns {"text": <tokenized>, "store": [{tag,tex}, ...]}. */
json_t *w20_adapterhtm_fd926b_latex_to_tokens(const char *text);

/* Expand sentinel tokens into <tag data-mx-maths="...">...</tag>. */
char *w20_adapterhtm_fd926b_tokens_to_mx_maths(const char *html, const json_t *store);

/* Turn sentinel tokens back into their $tex$ / $$tex$$ source text. */
char *w20_adapterhtm_fd926b_tokens_to_tex_source(const char *text, const json_t *store);

/* Allowlist-sanitize `html`; TeX tokens expand to data-mx-maths in text nodes
 * only (an attribute value reverts to its $source$). `store` may be NULL. */
char *w20_adapterhtm_fd926b_sanitize(const char *html, const json_t *store);

/* Remove unsafe raw HTML before Markdown conversion can escape it. */
char *w20_adapterhtm_fd926b_pre_sanitize_markdown(const char *text);

/* <a href> gate: control chars stripped, allowlisted scheme only. */
char *w20_adapterhtm_fd926b_safe_url(const char *value);

#endif /* NOC_W20_ADAPTERHTM_FD926B_H */
