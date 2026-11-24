#ifndef NOC_W20_PROMPTS_C0FCFD_H
#define NOC_W20_PROMPTS_C0FCFD_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/pet/generate/prompts.py (gate w20_prompts_c0fcfd).
 *
 * The module is pure prompt text: state action blurbs, style hints, the
 * chroma-key background contract and the two prompt builders. Everything the
 * Python module exports is reproduced here; nothing touches the network.
 */

/* PoP: w20_prompts_c0fcfd_style_hint @ agent/pet/generate/prompts.py:style_hint
 * `_STYLE_HINTS.get((style or "auto").strip().lower(), "")` -- never NULL, an
 * unknown style answers the empty string. NULL reads as None -> "auto". */
const char *w20_prompts_c0fcfd_style_hint(const char *style);

/* PoP: w20_prompts_c0fcfd_state_action @ agent/pet/generate/prompts.py:STATE_ACTIONS
 * `STATE_ACTIONS.get(state, "a simple idle pose")`. */
const char *w20_prompts_c0fcfd_state_action(const char *state);

/* PoP: w20_prompts_c0fcfd_spacing_spec @ agent/pet/generate/prompts.py:_spacing_spec
 * [per-pose width px, gap px] for a row of frame_count poses:
 *   slots = max(1, frame_count); slot_w = 1536 / slots;
 *   (round(slot_w * 0.7), max(48, round(slot_w * 0.3)))
 * Python rounds half-to-even, which is what rint() does here. */
json_t *w20_prompts_c0fcfd_spacing_spec(double frame_count);

/* PoP: w20_prompts_c0fcfd_base_variations @ agent/pet/generate/prompts.py:BASE_VARIATIONS
 * The six per-draft look nudges, in declaration order. */
json_t *w20_prompts_c0fcfd_base_variations(void);

/* PoP: w20_prompts_c0fcfd_build_base_prompt @ agent/pet/generate/prompts.py:build_base_prompt
 * `(concept or "a distinctive mascot creature").strip()`; NULL/blank falls back.
 * `variation` empty/NULL adds nothing. Returns a malloc'd UTF-8 string. */
char *w20_prompts_c0fcfd_build_base_prompt(const char *concept, const char *style,
                                           const char *variation);

/* PoP: w20_prompts_c0fcfd_build_row_prompt @ agent/pet/generate/prompts.py:build_row_prompt
 * `STATE_ACTIONS.get(state, "a simple idle pose")`, `(concept or "the mascot").strip()`
 * and the spacing spec above inlined as concrete pixel numbers. */
char *w20_prompts_c0fcfd_build_row_prompt(const char *state, double frame_count,
                                          const char *concept, const char *style);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_PROMPTS_C0FCFD_H */
