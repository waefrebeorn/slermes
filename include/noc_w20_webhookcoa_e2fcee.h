/*
 * noc_w20_webhookcoa_e2fcee.h -- public C API for the port of upstream Hermes
 *   gateway/platforms/webhook_coalesce.py (gate w20_webhookcoa_e2fcee).
 *
 * Ported: validate_coalesce_config with its three ValueError texts,
 * PendingEvent.prompt_with_note, the _PLACEHOLDER_RE test, group_key's
 * "route|rendered" shape (the render callback itself is the adapter seam),
 * key-template normalization and the enqueue bookkeeping (first_at, count,
 * superseded chain, quiet-window delay capped by max_wait). The asyncio timers
 * and the dispatch() call stay upstream.
 */
#ifndef NOC_W20_WEBHOOKCOA_E2FCEE_H
#define NOC_W20_WEBHOOKCOA_E2FCEE_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Startup validation of a route's `coalesce` block. Returns the ValueError text
 * for a bad block, NULL when the route is fine. Caller frees a non-NULL value. */
char *w20_webhookcoa_e2fcee_validate(const char *route_name, const json_t *route);

/* PendingEvent.prompt_with_note(): the prompt, plus the coalesce note once more
 * than one event has arrived. `event` is {"prompt":"...","count":int}.
 * Returns the string; caller frees. */
char *w20_webhookcoa_e2fcee_prompt_with_note(const json_t *event);

/* _PLACEHOLDER_RE.search(s) -- `\{[a-zA-Z0-9_.]+\}`. */
int w20_webhookcoa_e2fcee_placeholder_present(const char *s);

/* group_key after the render callback: "route|rendered", or NULL when an
 * unresolved placeholder survived (unrelated entities must not collapse). */
char *w20_webhookcoa_e2fcee_group_key(const char *route_name, const char *rendered);

/* key_template.strip(), wrapped in braces when it is a bare dotted field.
 * Caller frees. */
char *w20_webhookcoa_e2fcee_normalize_template(const char *key_template);

/* The enqueue arithmetic. `spec` is {"route_name","rendered","now","window",
 * "max_wait","delivery_id","existing":{"first_at","count","superseded",
 * "delivery_id"}|null}; window / max_wait null mean the module defaults.
 * Returns {"queued":bool,"first_at":num|null,"count":int|null,
 *          "superseded":[...]|null,"delay":num|null}. */
json_t *w20_webhookcoa_e2fcee_enqueue_plan(const json_t *spec);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_WEBHOOKCOA_E2FCEE_H */
