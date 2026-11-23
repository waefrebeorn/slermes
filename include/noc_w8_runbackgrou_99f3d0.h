#ifndef NOC_W8_RUNBACKGROU_99F3D0_H
#define NOC_W8_RUNBACKGROU_99F3D0_H

/*
 * Public C API for the port of gateway/run_background.py (gate w8_runbackgrou_99f3d0).
 * All exported symbols are runbackgrou_-prefixed to avoid collisions.
 */
#include "hermes_json.h"

/* PoP: runbackgrou_bg_prompt_preview @ gateway/run_background.py:_bg_prompt_preview
 * Collapses whitespace, then truncates with a single U+2026 ellipsis. */
json_t *runbackgrou_bg_prompt_preview(const char *prompt, int limit);

/* PoP: runbackgrou_complete_preview @ gateway/run_background.py:_run_background_task_inner
 * The header preview computed from the raw prompt: first 60 chars + "..." when longer. */
json_t *runbackgrou_complete_preview(const char *prompt);

/* PoP: runbackgrou_media_image_paths @ gateway/run_background.py:_run_background_task_inner
 * The image_paths comprehension: urls whose parallel media type startswith "image/". */
json_t *runbackgrou_media_image_paths(const json_t *urls, const json_t *media_types);

#endif /* NOC_W8_RUNBACKGROU_99F3D0_H */
