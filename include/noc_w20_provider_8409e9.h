#ifndef NOC_W20_PROVIDER_8409E9_H
#define NOC_W20_PROVIDER_8409E9_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/web/openai_native/provider.py (gate w20_provider_8409e9).
 *
 * The provider is a marker: it never searches. The port carries the auth
 * probe (over an auth.json body), the refusal payload and the picker schema.
 */

/* PoP: w20_provider_8409e9_has_credentials @ plugins/web/openai_native/provider.py:has_codex_credentials
 * True when `auth_text` (an auth.json body, BOM allowed) carries a usable
 * openai-codex access token: providers.openai-codex.tokens.access_token, else
 * any credential_pool.openai-codex entry with one. `file_exists` false is the
 * missing-file answer; unparseable JSON is False. */
int w20_provider_8409e9_has_credentials(const char *auth_text, int file_exists);

/* PoP: w20_provider_8409e9_search_unsupported @ plugins/web/openai_native/provider.py:OpenAINativeWebSearchProvider.search
 * search() is never reachable on a healthy native turn; reached, it answers
 * search_fail(_UNSUPPORTED_MSG) verbatim. */
json_t *w20_provider_8409e9_search_unsupported(void);

/* PoP: w20_provider_8409e9_setup_schema @ plugins/web/openai_native/provider.py:OpenAINativeWebSearchProvider.get_setup_schema
 * setup_schema(DISPLAY_NAME, "native", tag, ""): the `hermes tools` entry. */
json_t *w20_provider_8409e9_setup_schema(void);

/* PoP: w20_provider_8409e9_identity @ plugins/web/openai_native/provider.py:OpenAINativeWebSearchProvider
 * {"name": NAME, "display_name": DISPLAY_NAME, "extract": False}. */
json_t *w20_provider_8409e9_identity(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_PROVIDER_8409E9_H */
