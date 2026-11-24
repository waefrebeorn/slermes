/*
 * noc_w27_adapter_04abb5_p1.h - C surface for the text / url / identity
 * leaves of upstream plugins/platforms/buzz/adapter.py
 * (gate w27_adapter_04abb5_p1).
 *
 * Ported: the unresolved-mention escaper, the attachment filename sanitiser,
 * the attachment-origin normaliser, the relay-media URL predicate and the
 * markdown/bare media-reference finder + rewriter, the hex->npub encoder and
 * the module tuning constants.  Sockets, files and subprocesses are seams.
 */
#ifndef NOC_W27_ADAPTER_04ABB5_P1_H
#define NOC_W27_ADAPTER_04ABB5_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {content, error} -> escaped text, or null when no token matched. */
json_t *w27_adapter_04abb5_escape_mention(const json_t *in);

/* in: {value} -> a basename safe for cache files and agent context. */
json_t *w27_adapter_04abb5_safe_filename(const json_t *in);

/* in: {value} -> [host, port] or null. */
json_t *w27_adapter_04abb5_attachment_origin(const json_t *in);

/* in: {url} -> effective port or null. */
json_t *w27_adapter_04abb5_effective_port(const json_t *in);

/* in: {url, relay_url} -> bool. */
json_t *w27_adapter_04abb5_is_relay_media_url(const json_t *in);

/* in: {text, relay_url} -> {urls: [...], replacements: [[s,e,repl], ...]}. */
json_t *w27_adapter_04abb5_find_relay_media_refs(const json_t *in);

/* in: {text, replacements} -> text with the spans replaced and whitespace
 * collapsed the way the adapter does before dispatch. */
json_t *w27_adapter_04abb5_replace_media_refs(const json_t *in);

/* in: {hex} -> npub1... bech32 string, or null. */
json_t *w27_adapter_04abb5_hex_to_npub(const json_t *in);

/* The module/class tuning constants, as an object. */
json_t *w27_adapter_04abb5_constants(void);

#ifdef __cplusplus
}
#endif
#endif
