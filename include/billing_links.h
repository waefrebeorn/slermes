/* billing_links.h — opaque interface for the faithful C11 port of
 * hermes-agent/agent/billing_links.py
 *
 * Provider-agnostic billing/credit-recovery link resolution. Pure logic:
 * no IO, no network. The dataclass BillingBlock is exposed as an opaque
 * struct (billing_block_t); callers read fields through accessors and free
 * with billing_block_free(). This keeps the module self-contained and avoids
 * leaking the internal layout into every consumer.
 *
 * Reuses existing C helpers (no reimplementation):
 *   - url_host_matches()            (include/hermes_url_safety.h)
 *   - is_nous_inference_route()     (src/agent/conversation_loop.c)
 */

#ifndef SLERMES_BILLING_LINKS_H
#define SLERMES_BILLING_LINKS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque billing-wall descriptor shared across every surface (CLI/TUI/desktop).
 * is_nous is the routing bit: Nous has a first-class in-app billing surface,
 * so surfaces prefer that over billing_url; third-party providers have no
 * in-app flow, so billing_url is the deep link the user actually needs. */
typedef struct billing_block billing_block_t;

/* Construct a billing descriptor for a billing-classified failure.
 * `message` is guidance already assembled by the agent loop, carried through
 * unchanged. Caller owns the returned pointer; release with billing_block_free().
 * Returns NULL only on allocation failure. */
billing_block_t *billing_block_build(const char *provider,
                                     const char *base_url,
                                     const char *model,
                                     const char *message);

/* Free a block produced by billing_block_build(). Safe to call with NULL. */
void billing_block_free(billing_block_t *b);

/* Accessors (mirror the Python dataclass fields). Never return NULL for the
 * string fields — an empty string is returned instead. */
const char *billing_block_provider(const billing_block_t *b);
const char *billing_block_provider_label(const billing_block_t *b);
const char *billing_block_model(const billing_block_t *b);
const char *billing_block_billing_url(const billing_block_t *b);  /* may be "" */
bool         billing_block_is_nous(const billing_block_t *b);
const char *billing_block_message(const billing_block_t *b);

/* True when the failing route is the Nous-managed inference gateway. */
bool billing_links_is_nous_inference_route(const char *provider,
                                            const char *base_url);

/* Faithful to_dict(): returns the BillingBlock as a compact JSON string
 * (key order matches the Python dataclass asdict). Caller frees. */
char *billing_block_to_json(const billing_block_t *b);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_BILLING_LINKS_H */
