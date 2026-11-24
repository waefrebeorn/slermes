#ifndef NOC_W22_MEDIA_6187D8_H
#define NOC_W22_MEDIA_6187D8_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/_store/media.py (gate w22_media_6187d8).
 *
 * The module records deletion candidates inside the delete transaction: every
 * media reference an admission payload holds (plus the retained documents a
 * hosted prompt names) becomes a `gateway.media.retired.v1.<sha256(path)>` row,
 * and every archive a doomed session's messages point at becomes a
 * `gateway.spillover.retired.v1.<name>` row. The sqlite handle is the seam: the
 * C answers the exact rows the module would INSERT, in order.
 */

/* PoP: w22_media_6187d8_prefix @ persistence/sqlite/conversation/_store/media.py:PREFIX */
const char *w22_media_6187d8_prefix(void);

/* PoP: w22_media_6187d8_spillover_prefix @ persistence/sqlite/conversation/_store/media.py:SPILLOVER_PREFIX */
const char *w22_media_6187d8_spillover_prefix(void);

/* PoP: w22_media_6187d8_retire_media @ persistence/sqlite/conversation/_store/media.py:retire_media
 * `retire_media`'s INSERT OR IGNORE rows for one payload, in the source's
 * reference order:
 *   [{"key": PREFIX+sha256(path), "value": <reference>}, ...]
 * where the references are admission_media_references(payload), then the hosted
 * document references (only when `media_root` is non-NULL; `request_id` must
 * start with "hosted:" and a found path must sit at
 * "<media_root>/<64 lowercase hex>/<name>"), then
 * payload.api_turn_v1.media.
 * NULL when a reference carries no string "path" (upstream KeyError/TypeError).
 * Windows path flavour, as the upstream Path does on this host: a drive-rooted
 * absolute path compared case-insensitively, dot components dropped. */
json_t *w22_media_6187d8_retire_media(const json_t *payload, const char *request_id,
                                      const char *media_root);

/* PoP: w22_media_6187d8_spillover_records @ persistence/sqlite/conversation/_store/media.py:retire_spillover
 * `retire_spillover`'s rows for the message rows it selects (the SELECT is the
 * DB seam): `rows` is [{"content":..., "api_content":...}, ...].
 *   [{"key": SPILLOVER_PREFIX+name, "value": name}, ...]
 * over retirement_candidates_in_text(content, api_content): a `spillover/<name>`
 * pointer answers <name>, a `cache/pruned/<name>` pointer answers
 * "cache/pruned/<name>". A set upstream, so the answer is sorted by key; each
 * row's rows are independent. Non-string texts contribute nothing. An
 * absent/empty `rows` answers [] (nothing selected), a non-array NULL. */
json_t *w22_media_6187d8_spillover_records(const json_t *rows);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_MEDIA_6187D8_H */
