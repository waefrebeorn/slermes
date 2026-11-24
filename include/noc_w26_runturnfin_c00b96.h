/*
 * noc_w26_runturnfin_c00b96.h - C surface for upstream
 * gateway/run_turn_final_reconcile.py (gate w26_runturnfin_c00b96).
 *
 * Finishing a streamed preview in place when the turn's final was not confirmed
 * delivered. The adapter calls, the sleep and the async retry loop are seams;
 * what is ported is the value->value core: the flood wait an error asks for, the
 * "one edit carries the whole answer" length test, and the ownership predicate
 * that decides whether a preview may be edited at all.
 */
#ifndef NOC_W26_RUNTURNFIN_C00B96_H
#define NOC_W26_RUNTURNFIN_C00B96_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"error": <str|null>, "retry_after": <number|null>} -> seconds or null. */
json_t *w26_runturnfin_c00b96_flood_wait(const json_t *req);

/* req: {"content": <str>, "splits": bool, "base": bool, "limit": <int|null>,
 *       "len_fn": "len"|"utf16", "fmt": "none"|"amp"} -> bool */
json_t *w26_runturnfin_c00b96_carries_whole(const json_t *req);

/* req: {"chat_id": <str>, "content": <str|null>, "consumer": {
 *         "message_id": <any>, "turn_split_delivery": bool,
 *         "native_streaming", "delivery_ambiguous", "delivery_uncertain",
 *         "preview_uncertain", "egress_declined": bool,
 *         "adapter": <the carries_whole request|null>, "clean": "none"|"strip"}}
 * -> bool: one real preview of this answer is still ours to finish in place. */
json_t *w26_runturnfin_c00b96_owns_preview(const json_t *req);

/* The retry budget and the flood vocabulary, as an object. */
json_t *w26_runturnfin_c00b96_constants(void);

#ifdef __cplusplus
}
#endif
#endif
