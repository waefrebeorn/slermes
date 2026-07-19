/*
 * pet_atlas.h — Deterministic spritesheet assembly (faithful C11 port of
 * agent/pet/generate/atlas.py: frame-segmentation, fit-to-cell, and
 * transparency-residue logic, adapted from OpenAI's hatch-pet skill).
 *
 * Pure RGBA pixel-buffer operations. No external image library required
 * (the C tree ships no PIL/WebP), so we operate on a raw byte buffer.
 *
 * The module is self-contained: an opaque pet_img_t owns its pixel buffer.
 * All functions that return a new image transfer ownership to the caller,
 * who frees with pet_img_free().
 */

#ifndef PET_ATLAS_H
#define PET_ATLAS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Alpha at/below which a pixel is "background" for component detection. */
#define PET_ALPHA_FLOOR 16
/* Cell padding kept around a fitted sprite. */
#define PET_CELL_PAD 10
/* Side-lobe cutoff for fitted frames. */
#define PET_SIDE_LOBE_RATIO 0.18f

typedef struct {
    uint8_t r, g, b, a;
} rgba_t;

typedef struct {
    int   w;
    int   h;
    rgba_t *px;   /* row-major, w*h elements; opaque-owned */
} pet_img_t;

/* Allocate a w*h image (zeroed = fully transparent). Returns NULL on OOM. */
pet_img_t *pet_img_new(int w, int h);
/* Free an image returned by any pet_* function. */
void pet_img_free(pet_img_t *img);
/* Reference a sub-rectangle (no copy) — caller must not free the parent while
 * the view is in use. Only used internally; exposed for tests. */
pet_img_t pet_img_view(const pet_img_t *img, int x0, int y0, int w, int h);

/* ── background removal ─────────────────────────────────────────────── */

/* Euclidean RGB distance to a key colour. */
float pet_color_distance(int r, int g, int b, int kr, int kg, int kb);

/* True if the strip already carries a real alpha background (>=5% transparent
 * pixels with alpha at/below the floor). */
bool pet_has_transparency(const pet_img_t *img);

/* Sample the four corners, return the most common opaque colour.
 * Returns (0,255,0) if all corners are transparent. */
void pet_dominant_corner_color(const pet_img_t *img, int *out_r, int *out_g, int *out_b);

/* Build an L mask (255 where a pixel is within tol per-channel of key).
 * Returned image stores the mask value in its alpha channel; caller frees. */
pet_img_t *pet_near_key_mask(const pet_img_t *img, int kr, int kg, int kb, int tol);

/* Shave the 1px antialiased edge ring (3x3 min filter on alpha). Mutates img. */
void pet_defringe(pet_img_t *img);

/* Key out the flat background to transparent. If the image already has a
 * transparent background, just repairs internal alpha holes. chroma_key may be
 * NULL (uses dominant corner colour). threshold is the colour-distance cutoff.
 * Caller frees the returned image. */
pet_img_t *pet_remove_background(const pet_img_t *img, const int *chroma_key,
                                 float threshold);

/* Fill transparent islands fully enclosed by opaque sprite pixels. Mutates img. */
void pet_repair_internal_alpha_holes(pet_img_t *img);

/* ── frame extraction ──────────────────────────────────────────────── */

/* Crop to content, scale (NEAREST) to fit a padded cell, center on transparent.
 * Returns a CELL_WIDTH x CELL_HEIGHT image; caller frees. */
pet_img_t *pet_fit_to_cell(const pet_img_t *img, int cell_w, int cell_h);

/* Remove tiny separated left/right lobes before fitting a frame. Caller frees. */
pet_img_t *pet_drop_side_bleed(const pet_img_t *img);

/* Remove thin slot-spanning guide/floor/divider lines (<=4px, spans >=85%).
 * Caller frees. */
pet_img_t *pet_erase_long_axis_lines(const pet_img_t *img);

/* Connected opaque components as ((x0,y0,x1,y1), mass). Caller frees the
 * returned array (and each box's int[4]). `out_count` receives the count. */
typedef struct { int box[4]; int mass; } pet_component_t;
pet_component_t *pet_component_boxes(const pet_img_t *img, int *out_count);

/* Keep the slot's real subject; drop detached effects/noise. Caller frees. */
pet_img_t *pet_isolate_slot_subject(const pet_img_t *img);

/* ── column projection / registration ──────────────────────────────── */

/* Per-column alpha mass (mean alpha per column). Returns `int[w]`, caller frees. */
int *pet_column_profile(const pet_img_t *img);

/* Contiguous column spans whose alpha mass exceeds threshold. Returns array of
 * (left,right) pairs; caller frees. *out_count is the number of pairs. */
int *pet_content_runs(const int *profile, int n, int *out_count, int threshold);

/* Integer dx that best aligns prof onto ref by cross-correlation (window). */
int pet_best_shift(const int *ref, int nref, const int *prof, int nprof, int window);

/* Merge disconnected parts that clearly belong to one subject (same visual
 * row, small vertical gap). Returns a new array (caller frees); *out_count set. */
int *pet_merge_related_boxes(const int *boxes, int count, int *out_count);

/* Group component boxes into visual rows, sorted left→right. Returns a flat
 * array of boxes (caller frees); *out_count set; row grouping is by the
 * returned order (the public contract for callers is the ordered list). */
int *pet_group_component_rows(const int *boxes, int count, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* PET_ATLAS_H */
