/*
 * noc_w24_constants_b9be53.h - C surface for upstream agent/pet/constants.py
 * (gate w24_constants_b9be53).
 *
 * Pet sprite geometry, the animation-state taxonomy of both atlas shapes and
 * the three scale/column resolvers.  Nothing here touches a file, a sheet or a
 * terminal: every function is a pure function of the values handed in, so the
 * image loader stays on the caller's side of the seam.
 */
#ifndef NOC_W24_CONSTANTS_B9BE53_H
#define NOC_W24_CONSTANTS_B9BE53_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Frame geometry + scale bounds, mirrored from the module's literals. */
#define W24_CONSTANTS_B9BE53_FRAME_W 192
#define W24_CONSTANTS_B9BE53_FRAME_H 208
#define W24_CONSTANTS_B9BE53_FRAMES_PER_STATE 6
#define W24_CONSTANTS_B9BE53_LOOP_MS 1100
#define W24_CONSTANTS_B9BE53_DEFAULT_SCALE 0.33
#define W24_CONSTANTS_B9BE53_MIN_SCALE 0.1
#define W24_CONSTANTS_B9BE53_MAX_SCALE 3.0
#define W24_CONSTANTS_B9BE53_BASE_UNICODE_COLS 24
#define W24_CONSTANTS_B9BE53_UNICODE_MIN_COLS 16
#define W24_CONSTANTS_B9BE53_LEGACY_ROWS 8
#define W24_CONSTANTS_B9BE53_CODEX_ROWS 9
#define W24_CONSTANTS_B9BE53_ALIASES 7

/* clamp_scale(scale): max(MIN_SCALE, min(MAX_SCALE, scale)).  A non-number is
 * Python's TypeError: null is returned for it (the caller rejects the value). */
json_t *w24_constants_b9be53_clamp_scale(const json_t *scale);

/* cols_for_scale(scale): max(UNICODE_MIN_COLS, round(BASE * (scale or 0.33))). */
json_t *w24_constants_b9be53_cols_for_scale(const json_t *scale);

/* resolve_cols(scale, unicode_cols): the explicit positive column count, else
 * cols_for_scale(scale). */
json_t *w24_constants_b9be53_resolve_cols(const json_t *scale, const json_t *unicode_cols);

/* state_aliases_for(state): the accepted row-name aliases, always non-empty. */
json_t *w24_constants_b9be53_state_aliases_for(const json_t *state);

/* state_rows_for_grid(row_count): CODEX rows for >= 9 rows, else LEGACY rows;
 * anything int() refuses counts as 0 rows. */
json_t *w24_constants_b9be53_state_rows_for_grid(const json_t *row_count);

/* state_row_index(state, row_count): the row of the first matching alias, 0 when
 * none matches (never raises). */
json_t *w24_constants_b9be53_state_row_index(const json_t *state, const json_t *row_count);

/* The geometry/scale literals plus both row taxonomies and the alias table. */
json_t *w24_constants_b9be53_constants(void);

#ifdef __cplusplus
}
#endif
#endif
