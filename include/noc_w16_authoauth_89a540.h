#ifndef NOC_W16_AUTHOAUTH_89A540_H
#define NOC_W16_AUTHOAUTH_89A540_H

#include "hermes_json.h"

/* C home for upstream hermes_cli/auth_oauth_grants.py.
 * Pure single-use OAuth grant classification helpers. */

bool w16_authoauth_89a540_is_oauth_pool_payload(const json_t *entry);
bool w16_authoauth_89a540_is_forkable_pool_row(const char *provider_id, const json_t *entry);
json_t *w16_authoauth_89a540_block_tokens(const json_t *block);
bool w16_authoauth_89a540_is_pkce_row(const json_t *row);
json_t *w16_authoauth_89a540_sign(const json_t *x, const json_t *y);
json_t *w16_authoauth_89a540_constants(void);

#endif /* NOC_W16_AUTHOAUTH_89A540_H */
