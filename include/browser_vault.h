/*
 * browser_vault.h — the `browser_vault` tool.
 *
 * Two actions are implemented and proven end to end against a live page: `list` (what is in the vault,
 * in the shape the model sees) and `fill` (put a saved login into the page that is open now). `fill`
 * answers a refusal object rather than filling when the page is not an origin the item was saved for.
 *
 * The remaining upstream actions -- save_login, enter_code, unlock -- need the live-page supervisor and
 * a masked unlock prompt owned by the surface, and are not registered: a tool that offers a verb it
 * cannot perform teaches the model to trust a lie.
 */
#ifndef HERMES_BROWSER_VAULT_H
#define HERMES_BROWSER_VAULT_H

#include "hermes_json.h"

/* Registry handler: args {"action": "list"|"fill", "handle"?, "password"?, "page_origin"?}.
 * Returns a malloc'd JSON string (caller frees). */
char *browser_vault_tool(const char *args_json, const char *task_id);

/* The fill itself, without the tool wrapper, so it can be driven from a test harness.
 * base: the vault directory. handle: an item id, or NULL to take the single login saved for the origin.
 * password_override: a one-off password, or NULL to resolve the item's stored password.
 * origin_override: a page origin to use instead of asking the attached page (tests only).
 * Returns a JSON object: {success, item_id, origin, filled} or {success:false, error|refused}. */
json_t *browser_vault_fill(const char *base, const char *handle, const char *password_override,
                           const char *origin_override);

#endif /* HERMES_BROWSER_VAULT_H */
