/*
 * noc_w26_protocol_45f875.h - C surface for upstream agent/lsp/protocol.py
 * (gate w26_protocol_45f875).
 *
 * The pure core of the LSP JSON-RPC 2.0 framer: the Content-Length envelope,
 * the four envelope builders, the message classifier and the header/body
 * decode state machine.  The async stream itself is the seam -- the caller
 * feeds bytes and this file decides what they mean.
 */
#ifndef NOC_W26_PROTOCOL_45F875_H
#define NOC_W26_PROTOCOL_45F875_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* spec 3.17 #errorCodes */
#define W26_PROTOCOL_45F875_ERROR_CONTENT_MODIFIED (-32801)
#define W26_PROTOCOL_45F875_ERROR_METHOD_NOT_FOUND (-32601)

#define W26_PROTOCOL_45F875_MAX_HEADER_BYTES 8192
#define W26_PROTOCOL_45F875_MAX_BODY_BYTES (64LL * 1024LL * 1024LL)

/* req: {"obj": <any>} -> "Content-Length: N\r\n\r\n<body>" (compact UTF-8). */
json_t *w26_protocol_45f875_encode(const json_t *req);

/* req: {"kind": "notification"|"request"|"response"|"error", ...fields} ->
 * the JSON-RPC 2.0 envelope object. */
json_t *w26_protocol_45f875_make(const json_t *req);

/* req: {"msg": <any>} -> [kind, key]; kind in
 * request/response/notification/invalid, key is the id, the method or null. */
json_t *w26_protocol_45f875_classify(const json_t *req);

/* req: {"wire": "<framed bytes>"} -> {"messages": [...], "error": null|"<kind>"}.
 * error kinds: eof, header_block, non_ascii_header, malformed_header,
 * missing_content_length, non_integer_length, unreasonable_length,
 * truncated_body, non_utf8, invalid_json. */
json_t *w26_protocol_45f875_decode(const json_t *req);

/* The framer/error-code budget constants, as an object. */
json_t *w26_protocol_45f875_constants(void);

#ifdef __cplusplus
}
#endif
#endif
