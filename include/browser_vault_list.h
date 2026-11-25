/*
 * browser_vault_list.h — the `browser_vault action=list` result, ported.
 *
 * tools/browser_vault_tool.py builds one JSON object across every enabled backend: the local vault
 * always, external managers only when their CLI is installed, and a read-only browser profile only
 * when configured. Each item is a handle plus metadata; a password never appears, and the absence of
 * one is the tool's whole safety property.
 *
 * This port covers the backends the C binary can actually enumerate -- the local store, and the
 * plugin backends the registry can name. External managers are reported under "locked" rather than
 * silently dropped, which is what upstream does for a manager it cannot unlock here.
 */
#ifndef HERMES_BROWSER_VAULT_LIST_H
#define HERMES_BROWSER_VAULT_LIST_H

#include "hermes_json.h"

/* base: the vault directory (…/vault). can_prompt: whether this surface can ask the user to unlock a
 * manager, which decides the wording of the "locked" hint. */
json_t *browser_vault_list_json(const char *base, int can_prompt);

#endif /* HERMES_BROWSER_VAULT_LIST_H */
