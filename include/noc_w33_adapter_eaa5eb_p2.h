/*
 * noc_w33_adapter_eaa5eb_p2.h - C surface for upstream
 * plugins/platforms/irc/adapter.py, part 2 (gate w33_adapter_eaa5eb_p2).
 *
 * The identity/admission half of the IRC adapter: which inbound PRIVMSGs are
 * admitted (own echo, CTCP ACTION folding, channel addressing, allowlist) and
 * what text/chat they carry; how a 433 nick collision advances the nick; how
 * the out-of-process "-cron" identity is derived and retried; which target a
 * standalone send is allowed to write to and how its payload is planned; and
 * the env -> extra key derivation the platform registry seeds from.
 *
 * Every seam (the socket, the registration pump, the secret store, the clock)
 * stays in the caller; this file owns only what the values decide.
 */
#ifndef NOC_W33_ADAPTER_EAA5EB_P2_H
#define NOC_W33_ADAPTER_EAA5EB_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Adapter-prefix byte; most networks cap NICKLEN at 30. */
#define W33_ADAPTER_EAA5EB_P2_NICK_MAX 30
/* Base is capped at 24 so "-cron-<n>" retries stay inside NICKLEN. */
#define W33_ADAPTER_EAA5EB_P2_NICK_BASE_MAX 24
/* 432/433 retries before the standalone sender gives up. */
#define W33_ADAPTER_EAA5EB_P2_NICK_ATTEMPTS 5

/* {nick, sender, target, text, allowed_users: [str]} ->
 * {"dispatch": bool, "text", "chat_id", "chat_type", "user_id", "user_name"}
 * (upstream IRCAdapter._handle_privmsg admission, dispatch payload null when
 * the line is dropped). */
json_t *w33_adapter_eaa5eb_p2_privmsg_decision(const json_t *req);

/* {nickname, current_nick} -> {"nick", "command"} for the 433 branch of
 * _handle_line (nickname base + bounded numeric suffix, NICK line emitted). */
json_t *w33_adapter_eaa5eb_p2_next_nick_collision(const json_t *req);

/* nickname -> the stripped, 24-char-capped "-cron" base
 * (upstream _standalone_send nick_base). */
json_t *w33_adapter_eaa5eb_p2_standalone_nick_base(const json_t *req);

/* {nick_base, attempts} -> f"{nick_base}-cron[-<attempts>]"[:30]
 * (upstream _sa_register initial NICK and its retry NICK; attempts past
 * NICK_ATTEMPTS clamp to the last rung the ladder can reach). */
json_t *w33_adapter_eaa5eb_p2_standalone_nick(const json_t *req);

/* {nick_base, collisions} -> {"ok", "nick", "detail"}: the 432/433 retry
 * ladder, erroring past W33_ADAPTER_EAA5EB_P2_NICK_ATTEMPTS. */
json_t *w33_adapter_eaa5eb_p2_nick_retry(const json_t *req);

/* {server, channel, chat_id, port, nickname} -> {"target", "nick_base",
 * "error"}: the target a standalone send may write to (chat_id or channel,
 * control characters rejected) and the cron nick base. */
json_t *w33_adapter_eaa5eb_p2_standalone_target(const json_t *req);

/* {message, target} -> {"lines": [...], "error"}: the standalone payload plan
 * (control chars stripped per line, markdown flattened, byte-budget chunked). */
json_t *w33_adapter_eaa5eb_p2_standalone_lines(const json_t *req);

/* target -> bool(target) and target[0] in "#&+!"
 * (upstream _is_irc_channel). */
json_t *w33_adapter_eaa5eb_p2_is_channel(const json_t *req);

/* chat_id -> {"name", "type": "group"|"dm"} (upstream get_chat_info). */
json_t *w33_adapter_eaa5eb_p2_chat_info(const json_t *req);

/* env: {IRC_SERVER, IRC_CHANNEL, IRC_PORT, ...} -> null, or
 * {"server", "channel", + seeded port/nickname/use_tls/passwords,
 * "home_channel": {"chat_id", "name"}} (upstream _env_enablement). */
json_t *w33_adapter_eaa5eb_p2_env_enablement(const json_t *req);

/* {extra, env} -> {"server", "channel"} with env winning over config.yaml
 * (upstream _server_channel). */
json_t *w33_adapter_eaa5eb_p2_server_channel(const json_t *req);

/* {extra, env} -> bool(server and channel) (upstream validate_config). */
json_t *w33_adapter_eaa5eb_p2_config_ready(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
