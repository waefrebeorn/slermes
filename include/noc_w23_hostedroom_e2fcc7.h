/*
 * noc_w23_hostedroom_e2fcc7.h - C surface for upstream
 * gateway/hosted_room_attachments.py (gate w23_hostedroom_e2fcc7).
 *
 * The storage repository (rows + canonical bytes beside the room file), the
 * wall clock and the room log are seams: the caller supplies the bytes and the
 * facts, and this surface owns only what the module checks before it calls the
 * store -- the identifier rule, the magic-byte MIME sniff, the kind/MIME
 * agreement, and the strict bounded base64 the RPC accepts and returns.
 */
#ifndef NOC_W23_HOSTEDROOM_E2FCC7_H
#define NOC_W23_HOSTEDROOM_E2FCC7_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W23_HOSTEDROOM_E2FCC7_MAX_IDENTIFIER_CHARS 128
#define W23_HOSTEDROOM_E2FCC7_MAX_ATTACHMENT_BYTES 15000000
#define W23_HOSTEDROOM_E2FCC7_MAX_ATTACHMENTS_PER_MESSAGE 8
#define W23_HOSTEDROOM_E2FCC7_UNCOMMITTED_TTL_SECONDS 3600
#define W23_HOSTEDROOM_E2FCC7_DISBANDED_GRACE_SECONDS 900
#define W23_HOSTEDROOM_E2FCC7_CLASSIC_ATTACHMENT_TTL_SECONDS 604800

/* Every verb answers {"ok":true,"value":...} or {"ok":false,"error":"<message>"}
 * with the module's own AttachmentError text. */

/* req keys: value, label (default "room_id").  Strips and validates one
 * identifier: 1..128 chars of [A-Za-z0-9][A-Za-z0-9._:-]*. */
json_t *w23_hostedroom_e2fcc7_identifier(const json_t *req);

/* req keys: data_b64 (the upload bytes, base64 for the fixture transport).
 * The magic-byte sniff: image/png|jpeg|gif|bmp|webp, application/pdf|zip|gzip,
 * text/plain for NUL-free valid UTF-8, else application/octet-stream. */
json_t *w23_hostedroom_e2fcc7_sniff_mime(const json_t *req);

/* req keys: data_b64, kind ("image"|"pdf"|"file"), mime.  Raises (as an error
 * result) when the bytes disagree with the declared kind/MIME. */
json_t *w23_hostedroom_e2fcc7_validate_mime_kind(const json_t *req);

/* req keys: value.  decode_content_base64: bounded, strict, no data URLs;
 * answers the decoded bytes re-encoded as canonical base64. */
json_t *w23_hostedroom_e2fcc7_decode_b64(const json_t *req);

/* req keys: data_b64.  encode_content_base64: canonical ASCII base64. */
json_t *w23_hostedroom_e2fcc7_encode_b64(const json_t *req);

/* The shared limits and TTLs this module re-exports from its rules module. */
json_t *w23_hostedroom_e2fcc7_consts(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W23_HOSTEDROOM_E2FCC7_H */
