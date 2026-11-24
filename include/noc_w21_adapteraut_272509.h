#ifndef NOC_W21_ADAPTERAUT_272509_H
#define NOC_W21_ADAPTERAUT_272509_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/discord/adapter_authorization.py (gate
 * w21_adapteraut_272509).
 *
 * The mixin method `reauthorize_native_roles` is a live Discord/authority
 * round trip; what is ported here is its whole decision surface: the author
 * classification, the CSV->role-set coercion, and every early return of the
 * reauthorization walk, driven from a scripted client/guild/member. */

/* PoP: w21_adapteraut_272509_is_automated_message @ plugins/platforms/discord/adapter_authorization.py:is_automated_message
 * `bool(getattr(getattr(message, "author", None), "bot", False) or
 * isinstance(getattr(message, "webhook_id", None), int))`.
 * `message` is the decoded message object: {"author": {"bot": ...},
 * "webhook_id": ...}. A webhook post carries a numeric webhook_id even when
 * the author object has no bot flag, so both shapes are automated. Numbers in
 * this JSON shape are integral only when `is_py_int` holds (a JSON bool also
 * counts, exactly as Python's bool subclasses int). */
int w21_adapteraut_272509_is_automated_message(const json_t *message);

/* PoP: w21_adapteraut_272509_reauthorize @ plugins/platforms/discord/adapter_authorization.py:DiscordAuthorizationMixin.reauthorize_native_roles
 * The mind of `reauthorize_native_roles` against a scripted transport:
 *   case = {
 *     "role_items": [...],        # what _gate_csv_set() yields for allowed_roles
 *     "role_items_2": [...],      # optional: the second (post-fetch) policy read
 *     "chat_type": "dm"|"guild",  # source.chat_type
 *     "scope_id": ...,            # source.scope_id (non-dm guild id)
 *     "dm_guild_id": ...,         # _read_dm_role_auth_guild() for a dm
 *     "user_id": ...,             # source.user_id
 *     "client": true|false,       # self._client is not None
 *     "client_replaced": false,   # self._client changed identity during I/O
 *     "get_guild_raises": false,  # int(guild_id)/int(user_id) or get_guild raised
 *     "guild_found": true,        # get_guild() returned a guild
 *     "fetch_error": "HTTPException"|"TimeoutError"|"OSError"|null,
 *     "member": {"id":..., "guild_id":..., "role_ids":[...]}
 *   }
 * Answers 1 only for the final all-of: member id == uid, member's guild id ==
 * the requested guild, and any member role in the allowed role set. */
int w21_adapteraut_272509_reauthorize(const json_t *case_);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_ADAPTERAUT_272509_H */
