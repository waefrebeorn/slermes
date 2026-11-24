#ifndef NOC_W20_SERVER_266A25_H
#define NOC_W20_SERVER_266A25_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/google_meet/node/server.py (gate w20_server_266a25).
 *
 * The module is a WebSocket RPC server: token minting on disk, request
 * validation through plugins.google_meet.node.protocol, and a five-entry
 * handler table that drives process_manager.  Protocol validation and the
 * WebSocket loop stay upstream; the port covers the handler whitelist, the
 * `say` queueing decision, the token decision and the envelope shapes. */

/* PoP: w20_server_266a25_start_bot @ plugins/google_meet/node/server.py:_rpc_start_bot
 * kwargs = {k: payload[k] for k in _START_BOT_KEYS if k in payload}; a payload
 * without "url" raises _RpcError("missing 'url' in payload").
 * -> {"ok": bool, "error": str|null, "kwargs": {...}|null, "payload": <start_result>} */
json_t *w20_server_266a25_start_bot(const json_t *payload, const json_t *start_result);

/* PoP: w20_server_266a25_say @ plugins/google_meet/node/server.py:_rpc_say
 * ok=True means "enqueued", not "spoken": `enqueued` is only true when the
 * active session has an out_dir AND the queue append succeeded (`write_ok`).
 * -> {"ok": true, "enqueued": bool, "text": <payload.text or "">} */
json_t *w20_server_266a25_say(const json_t *payload, const json_t *active, int write_ok);

/* PoP: w20_server_266a25_dispatch @ plugins/google_meet/node/server.py:_RPC
 * -> {"kind":"ping"} | {"kind":"rpc","handler":<type>} |
 *    {"kind":"error","message":"unhandled type: <repr>"} */
json_t *w20_server_266a25_dispatch(const char *type);

/* PoP: w20_server_266a25_pong @ plugins/google_meet/node/server.py:NodeServer._handle_request
 * The ping envelope: {"type":"pong","id":msg.id,"payload":{display_name,ts}}. */
json_t *w20_server_266a25_pong(const json_t *msg, const char *display_name, double ts);

/* PoP: w20_server_266a25_error_envelope @ plugins/google_meet/node/protocol.py:make_error */
json_t *w20_server_266a25_error_envelope(const char *id, const char *reason);

/* PoP: w20_server_266a25_ensure_token @ plugins/google_meet/node/server.py:NodeServer.ensure_token
 * -> {"token": str|null, "generate": bool} -- the freshly minted value itself is
 * random, so only the decision is reported. */
json_t *w20_server_266a25_ensure_token(const json_t *token_data, const json_t *cached);

/* PoP: w20_server_266a25_start_bot_keys @ plugins/google_meet/node/server.py:_START_BOT_KEYS */
json_t *w20_server_266a25_start_bot_keys(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_SERVER_266A25_H */
