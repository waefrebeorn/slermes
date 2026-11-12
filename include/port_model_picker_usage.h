#ifndef SLERMES_PORT_MODEL_PICKER_USAGE_H
#define SLERMES_PORT_MODEL_PICKER_USAGE_H

#include <hermes_json.h>
#include <stddef.h>

/* hermes_cli/model_picker_usage.py — the reusable plan-usage chrome helpers
 * shared by the CLI radiolist and the gateway ``model.options`` inventory.
 * String outputs go into a caller-supplied buffer (128 bytes is ample). */

double mpu_clamp_fraction(double value);

/* "green" / "yellow" / "red" for a remaining fraction. */
const char *mpu_gauge_style(double left_fraction);

long mpu_filled_cells(double fraction, long width);

void mpu_render_bar(double fraction, long width, char *out, size_t outsz);

void mpu_format_percent_used(double used_fraction, char *out, size_t outsz);

/* resets_at_ms / now_ms are borrowed json values; a NULL now_ms means "now". */
void mpu_compact_duration(json_t *resets_at_ms, json_t *now_ms, char *out,
                          size_t outsz);
void mpu_format_resets_in(json_t *resets_at_ms, json_t *now_ms, char *out,
                          size_t outsz);

/* (recommended, other): the featured picks that are listed, plan order first. */
json_t *mpu_split_recommended(json_t *model_ids, json_t *featured, json_t **rest);
json_t *mpu_featured_first(json_t *model_ids, json_t *featured);

/* -1 unknown (None), 0 false, 1 true. */
int mpu_credits_pay(json_t *info, json_t *window);

int mpu_is_free_model(json_t *pricing_row);

void mpu_capitalize(const char *text, char *out, size_t outsz);

/* _get(obj, name, default): dict lookup, else attribute, else default. */
json_t *mpu_obj_get(json_t *obj, const char *name, json_t *dflt);

#endif /* SLERMES_PORT_MODEL_PICKER_USAGE_H */
