/*
 * noc_w21_mediadownl_8ef7a9.h -- public C API for the port of upstream
 *   plugins/platforms/matrix/media_download.py
 *
 * mautrix's Client.download_media buffers the whole body before the size cap can
 * be checked.  These two functions are the ported replacement: an inbound
 * `mxc://` body is rejected from a declared Content-Length, then streamed and
 * stopped the moment the running total crosses the limit.
 *
 * Only `download_mxc_capped` itself (HTTP session, token, logging) stays
 * upstream; it calls read_body_capped, which is what is ported here.
 */
#ifndef NOC_W21_MEDIADOWNL_8EF7A9_H
#define NOC_W21_MEDIADOWNL_8EF7A9_H

#include "hermes_json.h"

/* _CHUNK_SIZE (65536): the iter_chunked() size the download streams at. */
int w21_mediadownl_8ef7a9_chunk_size(void);

/*
 * read_body_capped(headers, chunks, limit) ->
 *   {"status":"ok","size":"<int>","body_hex":"<lowercase hex>"}
 *   {"status":"error","type":"...","message":"..."}
 *
 * headers : object of response headers (Content-Length / content-length)
 * chunks  : array of already-read body chunks (strings; byte length is used)
 * limit   : the cap in bytes (JSON number) -- MATRIX_MAX_MEDIA_BYTES
 *
 * An oversized declared length raises MatrixMediaTooLarge("... (N > M bytes)")
 * before any chunk is consumed; an oversized body raises
 * MatrixMediaTooLarge("... (> M bytes)") as soon as the running total crosses M.
 */
json_t *w21_mediadownl_8ef7a9_read_body_capped(const json_t *headers,
                                               const json_t *chunks,
                                               const json_t *limit);

#endif /* NOC_W21_MEDIADOWNL_8EF7A9_H */
