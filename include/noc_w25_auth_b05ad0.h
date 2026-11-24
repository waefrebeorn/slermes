/*
 * noc_w25_auth_b05ad0.h - C surface for upstream
 * plugins/model-providers/solstice/auth.py (gate w25_auth_b05ad0).
 *
 * The NAS-brokered Solstice sign-in.  Everything that touches the world --
 * the browser PKCE leg, the loopback redirect, the user's Nous access token,
 * the httpx POST to the portal -- is a seam.  What is ported is what the
 * values decide: which config fields the portal must serve before the flow
 * may start, which grant fields reach the token endpoint, and which failure
 * turns a rejected refresh into a forced re-login.
 */
#ifndef NOC_W25_AUTH_B05AD0_H
#define NOC_W25_AUTH_B05AD0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The NAS base URL the live module defaults to. */
#define W25_AUTH_B05AD0_NAS_BASE_URL "https://portal.nousresearch.com"
#define W25_AUTH_B05AD0_CONFIG_PATH "/api/oauth/gemini-auth/config"
#define W25_AUTH_B05AD0_EXCHANGE_PATH "/api/oauth/gemini-auth/exchange"
#define W25_AUTH_B05AD0_REFRESH_PATH "/api/oauth/gemini-auth/refresh"

/* The four grant fields the broker forwards (the client secret never leaves NAS). */
#define W25_AUTH_B05AD0_GRANT_BODY_FIELDS 4
/* The three config fields discover_client() insists on. */
#define W25_AUTH_B05AD0_CONFIG_FIELDS 3

/* payload: the object NAS served at the config path ->
 * {"ok", "value" {client_id, authorize_url, scope} | null, "code", "relogin",
 *  "request" {method, path, body}}.  A field that is absent, falsy or blank
 * after strip() makes the payload invalid (solstice_config_invalid). */
json_t *w25_auth_b05ad0_discover(const json_t *payload);

/* grant: the OAuth grant mapping; nas: {"status", "payload"} |
 * {"raise"} | {"token_error": true} -> the same envelope, with "request"
 * carrying the POST the broker composed (null when no request was made).
 * A refresh_answer >= 400 is remapped to invalid_grant + relogin_required. */
json_t *w25_auth_b05ad0_broker(const json_t *grant, const json_t *nas);

/* The module's literal tables: base URL, the three paths, the timeout. */
json_t *w25_auth_b05ad0_constants(void);

#ifdef __cplusplus
}
#endif
#endif
