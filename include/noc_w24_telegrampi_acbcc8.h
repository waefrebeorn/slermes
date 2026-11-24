/*
 * noc_w24_telegrampi_acbcc8.h - C surface for upstream
 * plugins/platforms/telegram/telegram_picker_labels.py (gate
 * w24_telegrampi_acbcc8).
 *
 * Readable, distinguishable labels for the Telegram /model picker.  Every
 * function here is a pure string/list -> string(s) transform: the shared
 * namespace is cut after a dot, over-budget ids lose their middle, look-alike
 * labels are re-shortened around the span that differs, and an ordinal is the
 * last resort so two buttons never read the same while selecting different
 * models.  Callback data (the catalog index) is not modelled at all.
 *
 * All lengths and slices are counted in Unicode code points, matching Python's
 * str semantics, so a multi-byte id truncates where Python truncates it.
 */
#ifndef W24_TELEGRAMPI_ACBCC8_H
#define W24_TELEGRAMPI_ACBCC8_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MODEL_LABEL_BUDGET: what a phone shows on a full-width button. */
#define W24_TELEGRAMPI_ACBCC8_MODEL_LABEL_BUDGET 44

/* model_ids: [str, ...] -> the shared namespace prefix ("" when there is none,
 * when the catalog is shorter than two ids, or when no id is over budget). */
json_t *w24_telegrampi_acbcc8_shared_label_prefix(const json_t *model_ids);

/* model_id, prefix, suffix -> the label: prefix stripped, tail after the last
 * '/', middle elided over budget, then " · suffix" when suffix is non-empty. */
json_t *w24_telegrampi_acbcc8_model_button_label(const json_t *model_id,
                                                 const json_t *prefix,
                                                 const json_t *suffix);

/* name, others, room -> name shortened to `room` keeping the span where it
 * differs from its look-alikes. */
json_t *w24_telegrampi_acbcc8_around_difference(const json_t *name,
                                                const json_t *others,
                                                const json_t *room);

/* model_ids: [str, ...], suffixes: {id: suffix} -> labels for the whole
 * catalog, unique within it. */
json_t *w24_telegrampi_acbcc8_labels(const json_t *model_ids,
                                     const json_t *suffixes);

/* The literal budget this module is tuned with. */
json_t *w24_telegrampi_acbcc8_constants(void);

#ifdef __cplusplus
}
#endif
#endif
