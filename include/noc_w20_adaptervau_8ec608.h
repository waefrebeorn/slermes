#ifndef NOC_W20_ADAPTERVAU_8EC608_H
#define NOC_W20_ADAPTERVAU_8EC608_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/discord/adapter_vault_code.py (gate
 * w20_adaptervau_8ec608): the Discord card for the browser vault's one-time
 * code prompt.
 *
 * Only the turn's author may open the modal, the code is never posted or
 * logged, and the modal title/label are cut to Discord's UTF-16 field budget.
 * discord.py, the shared _HermesView base and the i18n catalog stay upstream;
 * the localized notice/footer strings are the keys this card selects.
 */

/* _INPUT_ID ("hermes_vault_code_value"), _MODAL_TEXT_LIMIT (45) and
 * _HINT_LIMIT (300). */
const char *w20_adaptervau_8ec608_input_id(void);
int w20_adaptervau_8ec608_modal_text_limit(void);
int w20_adaptervau_8ec608_hint_limit(void);

/* PoP: w20_adaptervau_8ec608_frame_kept @ plugins/platforms/discord/adapter_vault_code.py:_DropVaultCodeFrames.filter
 * discord.py logs every raw frame at DEBUG, so a modal submit would put the
 * code in agent.log: drop frames carrying the code input id. */
int w20_adaptervau_8ec608_frame_kept(const char *message);

/* PoP: w20_adaptervau_8ec608_truncate_utf16 @ gateway/platforms/base.py:_prefix_within_utf16_limit
 * Longest prefix of the text whose UTF-16 length is <= limit; never splits a
 * surrogate pair. `limit` below zero counts as 0. */
json_t *w20_adaptervau_8ec608_truncate_utf16(const char *text, int limit);

/* PoP: w20_adaptervau_8ec608_check_auth @ plugins/platforms/discord/adapter_vault_code.py:VaultCodeView._check_auth
 * The base admission (allowlist, roles, pairing) still applies, and on top of
 * it only the user whose turn asked for the code may open the modal.
 * `interaction_user_id` is what the interaction's user carries (any JSON type;
 * a missing user reads as ""). */
int w20_adaptervau_8ec608_check_auth(int admitted, const json_t *interaction_user_id,
                                     const char *user_id);

/* PoP: w20_adaptervau_8ec608_submit_plan @ plugins/platforms/discord/adapter_vault_code.py:VaultCodeView.submit_code
 * The submit path: the gate first (a refused interaction resolves nothing), a
 * blank code defers and leaves the button live, an accepted code disables the
 * card and stops the view. `accepted` is what the injected submit callback
 * answered.
 *   {"gated":true} | {"deferred":true} |
 *   {"notice":key, "footer":key, "ephemeral":true, "disabled":true,
 *    "stopped":true, "resolved":true} */
json_t *w20_adaptervau_8ec608_submit_plan(const char *code, int admitted, int accepted);

/* PoP: w20_adaptervau_8ec608_prompt_content @ plugins/platforms/discord/adapter_vault_code.py:DiscordVaultCodeMixin.send_vault_code_prompt
 * The prompt body: the localized body text, plus a newline and the hint cut to
 * _HINT_LIMIT code points when a hint is given. */
json_t *w20_adaptervau_8ec608_prompt_content(const char *body_text, const char *hint);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_ADAPTERVAU_8EC608_H */
