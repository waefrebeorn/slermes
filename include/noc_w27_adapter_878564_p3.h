/*
 * noc_w27_adapter_878564_p3.h - C surface for upstream
 * plugins/platforms/photon/adapter.py (gate w27_adapter_878564_p3).
 *
 * Gate 3 of 5: the NORMALIZE ladder and the outbound payload BODIES.  The
 * normalize tuple is (text, type, media_urls, media_types) rendered as
 * {"text","type","media_urls","media_types"}; the media-cache write is a seam
 * (`cached_path` / a `cache` array of paths-or-null consumed in traversal order).
 */
#ifndef NOC_W27_ADAPTER_878564_P3_H
#define NOC_W27_ADAPTER_878564_P3_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _normalize_binary_payload: cached_path non-NULL is the seam's "bytes cached"
 * verdict; NULL means metadata-only → the marker text. */
json_t *w27_adapter_878564_p3_normalize_binary_payload(const json_t *payload,
                                                       const char *cached_path);

/* _normalize_group_content over a `cache` array (one cell per binary item). */
json_t *w27_adapter_878564_p3_normalize_group_content(const json_t *content,
                                                      const json_t *cache);

/* _normalize_content: the ladder by content type; unknown types get the
 * "[Photon content type not handled: …]" marker. */
json_t *w27_adapter_878564_p3_normalize_content(const json_t *content,
                                                const json_t *cache);

/* _attachment_body: /send-attachment body; optional keys only when supplied. */
json_t *w27_adapter_878564_p3_attachment_body(const char *space_id, const char *safe_path,
                                              const char *kind, const char *name,
                                              const char *mime_type, const char *caption);

/* _attachment_label: "[kind: name]".  Caller frees. */
char *w27_adapter_878564_p3_attachment_label(const char *kind, const char *name);

/* PhotonAdapter._sidecar_send's pure half: the ordered attempt plan
 * {"sent_text","rich_url","attempts":[{"path","body"[,"truncated"]}]} — the
 * rich-link attempt first when the text is a bare URL and markdown is on, then
 * the plain text send (markdown disabled after a rich-link failure). */
json_t *w27_adapter_878564_p3_send_plan(const char *space_id, const char *text, int richlink,
                                        int markdown, int markdown_enabled);

/* PhotonAdapter._sidecar_send_poll: {"ok","body","error","sent_text"} — title
 * required, at least two non-blank options. */
json_t *w27_adapter_878564_p3_send_poll_plan(const char *space_id, const char *title,
                                             const json_t *options);

#ifdef __cplusplus
}
#endif
#endif
