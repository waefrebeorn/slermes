/*
 * noc_w26_sessionmes_de1f3f.h - C surface for upstream
 * gateway/session_messaging_picks.py (gate w26_sessionmes_de1f3f).
 *
 * A viewer's model/reasoning pick on a messaging chat is admitted by two value
 * decisions before any authority call happens: whether the operation is a chat
 * pick at all (and the chat is not the LOCAL platform), and which config.yaml
 * key the chat's platform maps to.  The live session, its source, the authority,
 * the runner and the runtime row are seams fixed by the caller.
 */
#ifndef NOC_W26_SESSIONMES_DE1F3F_H
#define NOC_W26_SESSIONMES_DE1F3F_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* operation: str|null, platform: str, live: bool -> operation in CHAT_PICKS and
 * a live session exists and its platform is not LOCAL. */
json_t *w26_sessionmes_de1f3f_is_chat_pick(const json_t *operation, const json_t *platform,
                                           const json_t *live);

/* platform: str -> "cli" for LOCAL, else the platform's own value. */
json_t *w26_sessionmes_de1f3f_platform_key(const json_t *platform);

/* CHAT_PICKS, sorted, as an array. */
json_t *w26_sessionmes_de1f3f_constants(void);

#ifdef __cplusplus
}
#endif
#endif
