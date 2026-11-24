/*
 * noc_w24_protocol_13d3cf.h - C surface for upstream
 * plugins/google_meet/node/protocol.py (gate w24_protocol_13d3cf).
 *
 * The gateway <-> meet-node RPC envelope: the closed request-type set, the
 * ordered validation chain that produces the client-safe refusal reason, the
 * three envelope constructors and the compact JSON codec.
 *
 * Nothing here touches a socket: the source module builds and checks plain
 * dicts, and that is exactly what is ported.  ``uuid.uuid4()`` is the one
 * non-deterministic input -- it arrives as the ``uuid_hex`` seam, the value a
 * caller's uuid4().hex would supply.
 */
#ifndef W24_PROTOCOL_13D3CF_H
#define W24_PROTOCOL_13D3CF_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W24_PROTOCOL_13D3CF_TYPE_COUNT 6

/* _nonempty_str(value) -> bool. */
json_t *w24_protocol_13d3cf_nonempty_str(const json_t *value);

/* make_request(type, token, payload, req_id, uuid_hex) ->
 * {"type","id","token","payload"}, or {"error": <ValueError text>}.
 * A falsy req_id (null / "") falls back to uuid_hex, the uuid4().hex seam. */
json_t *w24_protocol_13d3cf_make_request(const json_t *type, const json_t *token,
                                         const json_t *payload, const json_t *req_id,
                                         const json_t *uuid_hex);

/* make_response(req_id, payload) -> {"type":"response","id","payload"} or
 * {"error":"payload must be a dict"}. */
json_t *w24_protocol_13d3cf_make_response(const json_t *req_id, const json_t *payload);

/* make_error(req_id, error) -> {"type":"error","id","error": str(error)}. */
json_t *w24_protocol_13d3cf_make_error(const json_t *req_id, const json_t *error);

/* encode(msg) -> the compact JSON text ("," / ":" separators). */
json_t *w24_protocol_13d3cf_encode(const json_t *msg);

/* decode(raw) -> {"ok":true,"envelope":{...}} or {"ok":false,"reason":<text>}.
 * The reason is the stable head of the source's ValueError text. */
json_t *w24_protocol_13d3cf_decode(const json_t *raw);

/* validate_request(msg, expected_token) -> [ok, reason]. */
json_t *w24_protocol_13d3cf_validate_request(const json_t *msg, const json_t *expected_token);

/* VALID_REQUEST_TYPES (sorted) and the envelope type literals, as an object. */
json_t *w24_protocol_13d3cf_constants(void);

#ifdef __cplusplus
}
#endif
#endif
