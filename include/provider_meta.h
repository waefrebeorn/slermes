/*
 * provider_meta.h — opaque API for provider display metadata + grouping.
 *
 * Faithful C port of the pure, network-free display-logic slice of
 * hermes_cli/models.py:
 *   - normalize_provider(alias)        (provider alias -> canonical slug)
 *   - provider_label(slug)             (slug/alias -> human label)
 *   - provider_group_for_slug(slug)    (slug -> group_id or "")
 *   - group_providers(slugs)           (fold flat slug list into picker rows)
 *
 * These are DISPLAY-ONLY helpers (provider pickers / /model keyboards). They
 * depend only on static tables (PROVIDER_LABELS / PROVIDER_ALIASES /
 * PROVIDER_GROUPS), never on the live model catalog or network.
 *
 * "auto" passes through normalize_provider() unchanged; provider_label("auto")
 * returns "Auto".
 *
 * Opaque structs + minimal includes. group_providers() returns a heap list
 * of rows the caller must free with provider_group_rows_free().
 *
 * PoP: normalize_provider         @ hermes_cli/models.py:normalize_provider
 * PoP: provider_label             @ hermes_cli/models.py:provider_label
 * PoP: provider_group_for_slug    @ hermes_cli/models.py:provider_group_for_slug
 * PoP: group_providers            @ hermes_cli/models.py:group_providers
 */

#ifndef PROVIDER_META_H
#define PROVIDER_META_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Normalize a provider alias/name to Hermes' canonical provider slug.
 * "auto" passes through unchanged. Caller must NOT free (returns a static
 * string or the input-derived canonical slug — see provider_meta note:
 * the returned pointer may point into internal static storage; copy it if
 * you need to keep it past the next call). */
const char *normalize_provider(const char *provider);

/* Human-friendly label for a provider id or alias.
 * Returns "Auto" for "auto"; falls back to the original (or "OpenRouter")
 * when unknown. Caller must free the returned string. */
char *provider_label(const char *provider);

/* Group id a slug belongs to, or "" if ungrouped. Never NULL. */
const char *provider_group_for_slug(const char *slug);

/* ───────────────────────────────────────────────────────────────────
 * group_providers — fold a flat slug list into picker rows.
 * ─────────────────────────────────────────────────────────────────── */

typedef enum {
    PROVIDER_ROW_SINGLE = 0,   /* ungrouped or 1-member group */
    PROVIDER_ROW_GROUP  = 1    /* 2+ member group */
} provider_row_kind_t;

typedef struct provider_row_t {
    provider_row_kind_t kind;
    char *slug;            /* for SINGLE: the slug. For GROUP: members[0] (first) */
    char *group_id;        /* for GROUP only; "" otherwise */
    char *label;           /* for GROUP only: group display label */
    char *description;     /* for GROUP only: group description */
    char **members;        /* for GROUP only: NULL-terminated array of member slugs */
    size_t n_members;
    struct provider_row_t *next;   /* linked list in input order */
} provider_row_t;

/* Fold an argv-style NULL-terminated slug list. Returns a linked list of
 * rows (input order preserved) or NULL on alloc failure. Free with
 * provider_group_rows_free(). */
provider_row_t *group_providers(const char *const *slugs);

/* Free a row list produced by group_providers(). Safe with NULL. */
void provider_group_rows_free(provider_row_t *rows);

#ifdef __cplusplus
}
#endif

#endif /* PROVIDER_META_H */
