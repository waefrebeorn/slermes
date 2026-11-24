#ifndef NOC_W22_ADAPTERCOM_02F6A6_H
#define NOC_W22_ADAPTERCOM_02F6A6_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/discord/adapter_component_auth.py
 * (gate w22_adaptercom_02f6a6).
 *
 * `_component_check_auth` / `component_chat_context` / `opener_refusal` are pure
 * decisions over attributes of a discord.py interaction, an allowlist snapshot
 * and a view. Four things they touch are seams, injected by the fixture rather
 * than read from the process:
 *
 *   - the profile secret-scope env read (`_scoped_gate_env`, which is
 *     `(os.getenv(name) or default).strip()` for a single-profile install) --
 *     passed as the `env` object;
 *   - the pairing store (`PairingStore().is_approved("discord", uid)`) -- the
 *     `paired` flag;
 *   - the per-call live authorization callback (`live_auth`) -- `live_auth`;
 *   - the `/update` pending-marker file read (`update_prompt_refusal`) and the
 *     i18n catalog (`refusal_text`) -- `update_authority` and the returned
 *     requester key.
 */

/* PoP: w22_adaptercom_02f6a6_chat_context @ plugins/platforms/discord/adapter_component_auth.py:component_chat_context
 * `[chat_type, chat_id, thread_id]` for a click: "dm" with no guild, "thread"
 * (both ids) when the channel is the thread class, else "group". `chat_id` is
 * `str(channel_id)`, or null when the interaction carries no channel id.
 *
 * interaction: {"channel_id": <scalar>|null, "guild": <any>|null,
 *               "channel_kind": <string>|null, "thread_cls": <string>}
 * Missing keys read as absent attributes; a null/absent interaction is a click
 * with no attributes at all. */
json_t *w22_adaptercom_02f6a6_chat_context(const json_t *interaction);

/* PoP: w22_adaptercom_02f6a6_check_auth @ plugins/platforms/discord/adapter_component_auth.py:_component_check_auth
 * 1 when the click is admitted, else 0 (fail closed). allow-all env flags win;
 * then a `*`/uid/connect-time allowlist hit gated by `live_auth`; then a role
 * allowlist hit (no role data at all fails closed); then pairing approval.
 *
 * spec: {"interaction": {"user": {"id": <scalar>, "roles": <array|string|number>|null}|null,
 *                        "channel_id": ..., "guild": ...},
 *        "allowed_user_ids": [<scalar>...]|null,
 *        "allowed_role_ids": [<scalar>...]|null,
 *        "live_auth": true|false|"none" (absent = no callback),
 *        "env": {"DISCORD_ALLOW_ALL_USERS": s, "GATEWAY_ALLOW_ALL_USERS": s,
 *                "GATEWAY_ALLOWED_USERS": s},
 *        "paired": <truthy>} */
int w22_adaptercom_02f6a6_check_auth(const json_t *spec);

/* PoP: w22_adaptercom_02f6a6_opener_requester @ plugins/platforms/discord/adapter_component_auth.py:opener_refusal
 * Who may not answer the card: the requester key `refusal_text` would render
 * (the opener's name, or "\0no-requester" as the literal text
 * "\\0no-requester"), or null when the view is not a prompt or the clicker is
 * allowed. A picker card is `prompt_authority` of its selection callback; the
 * `/update` prompt is the authority its pending marker records.
 *
 * view: {"answer_extra": {...}|null, "has_on_selected": <truthy>,
 *        "prompt_authority": {...}|null, "is_update_prompt": <truthy>,
 *        "update_authority": {...}|null}
 * interaction: {"user": {"id": <scalar>}|null} */
json_t *w22_adaptercom_02f6a6_opener_requester(const json_t *view, const json_t *interaction);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_ADAPTERCOM_02F6A6_H */
