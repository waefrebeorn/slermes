/*
 * browser_vault_fill_plan.h — the decision layer in front of a vault fill.
 *
 * A fill writes a stored secret into a page, so two questions are answered before any value is even
 * resolved: is this page an origin the item was saved for, and which control receives a login's
 * (non-secret) identifier. Both are pure -- the live page only supplies "what origin am I on" -- and
 * both are security decisions, which is why they are separated from the CDP work and verified on
 * their own.
 */
#ifndef HERMES_BROWSER_VAULT_FILL_PLAN_H
#define HERMES_BROWSER_VAULT_FILL_PLAN_H

#include "hermes_json.h"

/* tools/browser_vault_tool.py:_bound_page_origin, minus the page lookup.
 *
 * saved_origins: the exact origins the item may be filled on (never wildcards, never parent domains).
 * page_origin:   the origin of the page that is open, or NULL when it could not be determined.
 * item_id, kind: named in the "no bound origin" message, because upstream names them and a message
 *                that omits which item is unfixable for the reader.
 *
 * Returns the origin string when the fill may proceed, otherwise upstream's refusal object -- so a
 * caller can hand the result straight to the model. */
json_t *vault_origin_guard(const json_t *saved_origins, const char *page_origin,
                           const char *item_id, const char *kind);

/* tools/browser_vault_tool.py:_identifier_fill. classified: ClassifiedLoginControl-shaped objects
 * ({index, token, score, form_index}); password_fill: the chosen password fills. Returns an array of
 * at most one {index, token, value} fill. */
json_t *vault_identifier_fill(const json_t *classified, const json_t *password_fill,
                              const char *identifier);

#endif /* HERMES_BROWSER_VAULT_FILL_PLAN_H */
