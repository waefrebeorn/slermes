/*
 * noc_w23_inlinepick_18ba58.h - C surface for upstream
 * plugins/platforms/telegram/inline_picker.py (gate w23_inlinepick_18ba58).
 *
 * Telegram inline command picker: the searchable catalog behind
 * "@bot <query>" (50 results per answerInlineQuery call).  The command/skill
 * registry is the seam: the caller supplies the (name, description) rows that
 * telegram_bot_commands() and _collect_gateway_skill_entries() would have
 * returned, and this file owns the dedupe, the ranking (prefix > name
 * substring > description) and the paging arithmetic.
 *
 * String semantics mirror CPython: .strip()/.strip(None) whitespace, ASCII
 * .lower(), and [:N] slicing counted in CODEPOINTS, not bytes.
 */
#ifndef NOC_W23_INLINEPICK_18BA58_H
#define NOC_W23_INLINEPICK_18BA58_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PAGE_SIZE / CACHE_TIME_SECONDS: Telegram caps one inline answer at 50. */
#define W23_INLINEPICK_18BA58_PAGE_SIZE 50
#define W23_INLINEPICK_18BA58_CACHE_SECONDS 10

int w23_inlinepick_18ba58_page_size(void);
int w23_inlinepick_18ba58_cache_seconds(void);

/* collect_inline_catalog with the registry stubbed.
 * world keys: registry (true = the commands registry imports; false = the
 * import raises and the catalog stays empty), bot[][2] (name, description),
 * skills[][2] (entry[0], entry[1] of the skill/plugin rows), bot_error,
 * skills_error (that collection call raises and is swallowed).
 * Returns the catalog array of {"name","description"}; first occurrence wins,
 * a falsy name is skipped and a falsy description becomes "". */
json_t *w23_inlinepick_18ba58_collect(const json_t *world);

/* filter_catalog.  world keys: catalog[] (rows carrying at least "name" and
 * optionally "description"), term.  Returns the matched rows, in rank order,
 * as the caller handed them over (extra keys preserved). */
json_t *w23_inlinepick_18ba58_filter(const json_t *world);

/* build_inline_results.  world keys: catalog[], query, offset (string or
 * number; anything int() refuses reads as 0), page_size (default PAGE_SIZE).
 * Returns {"results":[{id,title,description,message_text}],"next_offset":""};
 * next_offset is "" on the last page. */
json_t *w23_inlinepick_18ba58_build(const json_t *world);

#ifdef __cplusplus
}
#endif
#endif
