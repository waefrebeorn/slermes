/*
 * noc_w27_adapter_878564_p1.h - C surface for upstream
 * plugins/platforms/photon/adapter.py (gate w27_adapter_878564_p1).
 *
 * Gate 1 of 5: the TEXT half — markdown→plain-text shaping, the two URL
 * classifiers and the rich-link body text.  Nothing here touches the network,
 * the media cache or the clock: every function is a pure string transform.
 *
 * Seam: PHOTON_MARKDOWN (the markdown kill-switch) is a caller-supplied flag.
 */
#ifndef NOC_W27_ADAPTER_878564_P1_H
#define NOC_W27_ADAPTER_878564_P1_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* strip_markdown(text, keep_link_targets): bold/italic/underscore/code/heading
 * removal, the markdown-link rule, the newline squeeze and .strip().
 * Caller frees. */
char *w27_adapter_878564_p1_strip_markdown(const char *text, bool keep_link_targets);

/* PhotonAdapter.format_message: verbatim when markdown is enabled, else the
 * keep-link-targets strip.  Caller frees. */
char *w27_adapter_878564_p1_format_message(const char *content, bool markdown_enabled);

/* _sidecar_payload_text: verbatim only when the sidecar will use the markdown
 * builder (markdown on AND no raw URL in the text), else stripped.
 * Caller frees. */
char *w27_adapter_878564_p1_sidecar_payload_text(const char *text, bool send_markdown);

/* _url_only_candidate: the stripped text when it is exactly one http(s) URL with
 * a non-empty netloc, else NULL.  Caller frees. */
char *w27_adapter_878564_p1_url_only_candidate(const char *text);

/* _richlink_candidate: the URL candidate only when markdown is enabled. */
char *w27_adapter_878564_p1_richlink_candidate(const char *text, bool markdown_enabled);

/* _format_richlink_content: "title\nsummary\nurl" (summary dropped when equal to
 * the title), or the no-URL placeholder.  Caller frees. */
char *w27_adapter_878564_p1_format_richlink_content(const json_t *content);

#ifdef __cplusplus
}
#endif
#endif
