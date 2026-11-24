/*
 * noc_w28_files_ee43ed_p3.h - C surface for the QUERY PARSING / COERCION /
 * PAYLOAD DECODING partition of upstream hermes_cli/web_routers/files.py
 * (gate w28_files_ee43ed_p3).
 *
 * What an inbound request MEANS before it touches the disk: data-URL decoding
 * with strict base64, image magic sniffing, the media-proxy host/content-type
 * policy (allowlist + redirect stays-on-host + the response verdict), profile
 * and terminal-cwd coercion, and the read-text response flags.
 */
#ifndef NOC_W28_FILES_EE43ED_P3_H
#define NOC_W28_FILES_EE43ED_P3_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {data_url, max_bytes} -> {ok, mime_type, data[], status, detail}. base64 is
 * validated as base64.b64decode(validate=True) does; the length cap is 413. */
json_t *w28_files_ee43ed_p3_decode_data_url(const json_t *req);

/* The magic-byte sniff: .webp / .png / .jpg / .gif / .bmp, else null. */
json_t *w28_files_ee43ed_p3_chat_image_extension(const json_t *bytes);

/* {data_url, max_bytes, cap} -> decoded bytes + mime + ext, or the 400/413. */
json_t *w28_files_ee43ed_p3_decode_chat_image_upload(const json_t *req);

/* The media-proxy host allowlist (fal.media / fal.run / *.fal.media / ...). */
bool w28_files_ee43ed_p3_media_proxy_host_allowed(const char *host);

/* {url} -> {ok, host, suffix, status, detail}: scheme, host and extension gates. */
json_t *w28_files_ee43ed_p3_media_proxy_target(const json_t *req);

/* The response Content-Type allowlist (params stripped, lowercased). */
bool w28_files_ee43ed_p3_media_proxy_content_type_ok(const char *content_type);

/* {url, chain:[{status, location}]} -> {ok, hops, status, detail}: each redirect
 * hop must stay on the allowlist (403 otherwise), at most 3 hops followed. */
json_t *w28_files_ee43ed_p3_media_proxy_redirect(const json_t *req);

/* {status, content_type, size, max_bytes} -> the 502/415/413 verdict. */
json_t *w28_files_ee43ed_p3_proxy_response_verdict(const json_t *req);

/* (profile or "current").strip().lower() or "current". */
void w28_files_ee43ed_p3_profile_key(const char *profile, char *out, size_t cap);

/* terminal.cwd placeholders that count as unset ("", ".", "auto", "cwd"). */
bool w28_files_ee43ed_p3_default_cwd_placeholder(const char *raw);

/* {name, size, data[], preview_max} -> {binary, byteSize, truncated, language, mimeType}. */
json_t *w28_files_ee43ed_p3_read_text_meta(const json_t *req);

/* {suffix, under_roots, is_file, size, max_bytes} -> GET /api/media gates in order:
 * 415 type, 403 roots, 404 missing, 413 too large. */
json_t *w28_files_ee43ed_p3_media_suffix_verdict(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
