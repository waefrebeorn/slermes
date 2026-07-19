/*
 * pet_prompts.h — public API for the pure agent/pet/generate/prompts.py
 * prompt builders. Opaque, minimal includes.
 */

#ifndef PET_PROMPTS_H
#define PET_PROMPTS_H

#include <stddef.h>

/* Style hint string (malloc'd; "" for unknown style).
 * (PoP: style_hint) */
char *pet_prompts_style_hint(const char *style);

/* (per-pose width px, gap px) for a row of frame_count poses.
 * (PoP: _spacing_spec) */
void pet_prompts_spacing_spec(int frame_count, int *out_pose_px, int *out_gap_px);

/* Base look prompt (malloc'd). (PoP: build_base_prompt) */
char *pet_prompts_build_base(const char *concept, const char *style, const char *variation);

/* Row strip prompt (malloc'd). (PoP: build_row_prompt) */
char *pet_prompts_build_row(const char *state, int frame_count, const char *concept, const char *style);

#endif /* PET_PROMPTS_H */
