/*
 * noc_w29_adapter_1c52a4_p1.h - C surface for upstream
 * plugins/platforms/teams/adapter.py (gate w29_adapter_1c52a4_p1).
 *
 * The transport/security/config half of the Teams adapter: which URLs may
 * receive the bot's bearer token, the Bot Framework token request shape, the
 * conversation-ID charset, the require_mention opt-in and the default-deny
 * approval gate, plus the module constants those decisions are made against.
 *
 * Every network seam stays in the caller: the aiohttp webhook server, App.send
 * / App.reply, the STS token POST, attachment downloads.  The i18n catalog and
 * the profile-scoped env reads are passed in as resolved values.
 */
#ifndef NOC_W29_ADAPTER_1C52A4_P1_H
#define NOC_W29_ADAPTER_1C52A4_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _is_allowed_https_host(url, check_port=...) -> 1/0.  https only, host must be
 * one of the Bot Framework connector hosts; with check_port the port must be
 * absent or 443. */
int w29_adapter_1c52a4_p1_allowed_host(const char *url, int check_port);

/* _is_botframework_attachment_url(url) -> 1/0 (allowed host + port 443 only). */
int w29_adapter_1c52a4_p1_attachment_url(const char *url);

/* _validate_teams_service_url(raw) -> owned JSON string with a trailing "/", or
 * JSON null when the host is not on the allowlist. */
json_t *w29_adapter_1c52a4_p1_service_url(const char *raw);

/* ^[A-Za-z0-9:@\-_.]+$ over the Bot Framework conversation / tenant ID. */
int w29_adapter_1c52a4_p1_conv_id_valid(const char *chat_id);

/* _bf_token_request(tenant_id, client_id, client_secret) ->
 * {"url": ..., "form": {grant_type, client_id, client_secret, scope}}. */
json_t *w29_adapter_1c52a4_p1_token_request(const char *tenant_id,
                                           const char *client_id,
                                           const char *client_secret);

/* TeamsAdapter._parse_require_mention: env TEAMS_REQUIRE_MENTION first (when
 * non-blank), then extra["require_mention"], else false.  env may be NULL. */
int w29_adapter_1c52a4_p1_require_mention(const json_t *extra, const json_t *env);

/* TeamsAdapter._card_action_denied: allow-all opt-in, else the CSV allowlist
 * ("*" wildcard).  deny_text / unauth_text are the resolved catalog strings.
 * Returns an owned JSON string, or JSON null when the click is allowed. */
json_t *w29_adapter_1c52a4_p1_card_action_denied(const json_t *env,
                                                const char *aad_object_id,
                                                const char *user_id,
                                                const char *deny_text,
                                                const char *unauth_text);

/* Module + TeamsAdapter class constants, as an object. */
json_t *w29_adapter_1c52a4_p1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
