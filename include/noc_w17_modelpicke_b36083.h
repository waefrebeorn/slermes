/*
 * noc_w17_modelpicke_b36083.h -- public C API for the plan-usage chrome helpers
 * of hermes_cli/model_picker_usage.py (wave17 gate w17_modelpicke_b36083).
 */
#ifndef NOC_W17_MODELPICKE_B36083_H
#define NOC_W17_MODELPICKE_B36083_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

double w17_modelpicke_b36083_clamp_fraction(double value);
const char *w17_modelpicke_b36083_gauge_style(double left_fraction); /* red/yellow/green */
long w17_modelpicke_b36083_filled_cells(double fraction, long width);
void w17_modelpicke_b36083_render_bar(double fraction, long width, char *out, size_t outsz);
void w17_modelpicke_b36083_format_percent_used(double used_fraction, char *out, size_t outsz);
/* now_ms NULL means "now" in Python; pass an explicit value for deterministic ports. */
void w17_modelpicke_b36083_compact_duration(json_t *resets_at_ms, json_t *now_ms,
                                            char *out, size_t outsz);
void w17_modelpicke_b36083_format_resets_in(json_t *resets_at_ms, json_t *now_ms,
                                            char *out, size_t outsz);
/* Returns head (fresh array); when rest is non-NULL receives the other array. */
json_t *w17_modelpicke_b36083_split_recommended(json_t *model_ids, json_t *featured,
                                                json_t **rest);
json_t *w17_modelpicke_b36083_featured_first(json_t *model_ids, json_t *featured);
/* -1 unknown(None), 0 false, 1 true */
int w17_modelpicke_b36083_credits_pay(json_t *info, json_t *window);
int w17_modelpicke_b36083_is_free_model(json_t *pricing_row);
void w17_modelpicke_b36083_capitalize(const char *text, char *out, size_t outsz);
json_t *w17_modelpicke_b36083_obj_get(json_t *obj, const char *name, json_t *dflt);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_MODELPICKE_B36083_H */
