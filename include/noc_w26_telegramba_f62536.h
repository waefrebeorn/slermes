/*
 * noc_w26_telegramba_f62536.h - C surface for upstream
 * plugins/platforms/telegram/telegram_batching.py (gate w26_telegramba_f62536).
 *
 * The value core of inbound album/photo-burst merging: the inline-flag padding
 * that keeps each attachment's inlining semantics, the command-caption lift that
 * keeps an album's one caption parseable, and the per-item text merge.  The
 * event objects, the routing adapter, the batch dict and the caption join are
 * seam inputs fixed by the caller.
 *
 * merge_caption is the fixed join ``(a + "\n\n" + b) if a else b``.
 */
#ifndef NOC_W26_TELEGRAMBA_F62536_H
#define NOC_W26_TELEGRAMBA_F62536_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* inlined: [bool|null]|null, urls: [any] ->
 * list(inlined or []) + [None] * (len(urls) - len(flags)). */
json_t *w26_telegramba_f62536_pad_inline_flags(const json_t *inlined, const json_t *urls);

/* captions, incoming: str|null -> the two captions joined command-first, or
 * null when a later item does not carry the album's command caption. */
json_t *w26_telegramba_f62536_command_first(const json_t *captions, const json_t *incoming);

/* text, incoming: str|null, notes: [str] -> the merged text, or null. */
json_t *w26_telegramba_f62536_merged_text(const json_t *text, const json_t *incoming,
                                          const json_t *notes);

/* The private event attributes the merge keeps, as an object. */
json_t *w26_telegramba_f62536_constants(void);

/* caption_is_command, ported locally: the module may only import the published
 * plugin API, so it carries its own predicate. */
int w26_telegramba_f62536_caption_is_command(const char *caption);

#ifdef __cplusplus
}
#endif
#endif
