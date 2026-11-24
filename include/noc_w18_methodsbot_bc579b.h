#ifndef NOC_W18_METHODSBOT_BC579B_H
#define NOC_W18_METHODSBOT_BC579B_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tui_gateway/methods_bot_relay.py (gate w18_methodsbot_bc579b).
 * The bot_relay.reply handler's own control flow: id coercion/trim, the 4093
 * refusal, the 4094/5095 exception boundaries and the write_reply field shaping.
 * External write_reply / root resolution are stubbed by the oracle and injected. */

/* PoP: w18_methodsbot_bc579b_reply @ tui_gateway/methods_bot_relay.py:_ (bot_relay.reply)
 * mode: "ok" | "valueerror" | "error" (stub write_reply outcome); msg: str(exception).
 * Returns {"rpc":<ok|err payload>,"written":<recorded fields|null>}. */
json_t *w18_methodsbot_bc579b_reply(const char *rid, const json_t *params,
                                    const char *mode, const char *msg);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_METHODSBOT_BC579B_H */
