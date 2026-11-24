#ifndef NOC_W20_CONTEXTCOM_971C8D_H
#define NOC_W20_CONTEXTCOM_971C8D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/context_compressor_typed_input.py (gate w20_contextcom_971c8d).
 *
 * Role-typed summarizer input: compacted turns travel in their real roles, so
 * text a tool printed can only ever arrive as tool output. Only the pure
 * request shaper is exposed here; the caller's sampler and tool registry stay
 * in Python.
 */

/* PoP: w20_contextcom_971c8d_typed_summary_messages @ agent/context_compressor_typed_input.py:typed_summary_messages
 * OpenAI-shape messages for the sampled `slices` (start index, display
 * records) of `turns`.
 *   turns       array of dicts ({"role", "content", "tool_call_id", "tool_calls"})
 *   records     array of the serializer's redacted/elided text, 1:1 with turns
 *   slices      array of [start_index, [shown record strings]]
 *   real_user   array of booleans, 1:1 with turns: is_real_user(turns[i])
 *   render_args object mapping a raw tool-call argument string to the
 *               serializer's redact-and-cap rendering (absent -> identity)
 * Synthetic harness user turns, orphaned tool results and elision markers
 * become assistant text. Never NULL. */
json_t *w20_contextcom_971c8d_typed_summary_messages(const json_t *turns,
                                                     const json_t *records,
                                                     const json_t *slices,
                                                     const json_t *real_user,
                                                     const json_t *render_args);

/* PoP: w20_contextcom_971c8d_call_arguments @ agent/context_compressor_typed_input.py:_call_arguments
 * Tool-call arguments as a JSON object string: `raw` when it already parses to
 * a dict, else json.dumps({"arguments": raw}, ensure_ascii=False). `raw` is
 * passed through `render_args` first (identity when absent/not an object). */
json_t *w20_contextcom_971c8d_call_arguments(const json_t *raw,
                                             const json_t *render_args);

/* PoP: w20_contextcom_971c8d_typed_turns_pointer @ agent/context_compressor_typed_input.py:TYPED_TURNS_POINTER
 * The pointer sentence appended after the typed turns. */
const char *w20_contextcom_971c8d_typed_turns_pointer(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_CONTEXTCOM_971C8D_H */
