/*
 * noc_w24_display_330acd.h - C surface for upstream tools/connectors/display.py
 * (gate w24_display_330acd).
 *
 * The browsable connector card's value core: which connectors the client
 * reports as connected, what a typed answer does to the operation (say it,
 * settle it), the frozen result the card hands back, and the off-desktop note.
 * The tool thread, the card renderer and the waiting loop stay in the caller.
 */
#ifndef W24_DISPLAY_330ACD_H
#define W24_DISPLAY_330ACD_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of SettleReason members. */
#define W24_DISPLAY_330ACD_SETTLE_REASONS 4

/* [{"connector": name, "connected": bool}, ...] -> sorted, lower-cased names of
 * the connected entries. */
json_t *w24_display_330acd_connected_slugs(const json_t *items);

/* raw answer text -> {"said", "settled", "settled_by"}: a typed word is stored
 * and settles the operation, as does an explicit settled_by == "continue". */
json_t *w24_display_330acd_apply_answer(const json_t *raw);

/* (items, before, suggested, said, settled_by) -> the card's result:
 * {"connected", "newly_connected", "suggested"[, "said"][, "timed_out"]}. */
json_t *w24_display_330acd_result(const json_t *items, const json_t *before,
                                  const json_t *suggested, const json_t *said,
                                  const json_t *settled_by);

/* OFF_DESKTOP_NOTE, the operation deadline and the SettleReason table. */
json_t *w24_display_330acd_constants(void);

#ifdef __cplusplus
}
#endif
#endif
