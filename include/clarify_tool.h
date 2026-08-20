/*
 * clarify_tool.h — Interactive clarifying-question schema, validation, and
 * batch dispatch for the clarify tool.
 *
 * Faithful port of tools/clarify_tool.py. The platform-specific UI interaction
 * is injected by the caller via a `clarify_callback` (mirrors the Python
 * module's callback parameter); all schema/validation/normalization and result
 * JSON assembly are implemented here in C11.
 *
 * Memory contract: every function that returns a `char *` returns a heap buffer
 * the caller owns and must `free()`. The callback receives `const char *`
 * (owned by the engine) and returns a `char *` it owns (the engine frees it).
 */

#ifndef CLARIFY_TOOL_H
#define CLARIFY_TOOL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLARIFY_MAX_CHOICES 4
#define CLARIFY_MAX_QUESTIONS 5

/* Timeout sentinel returned to the agent when the user never answers. */
#define CLARIFY_TIMEOUT_RESPONSE                                              \
    "The user did not provide a response within the time limit. "            \
    "Use your best judgement to make the choice and proceed."

/* Suffix appended to the first choice to mark the agent's recommendation. */
#define CLARIFY_RECOMMENDED_LABEL "(Recommended)"

/*
 * Platform callback. Invoked with the question text, the (already flattened &
 * recommended-decorated) choices, and the multi_select flag. Returns the raw
 * user response (heap string, freed by the engine) or NULL on timeout/skip.
 *
 * `batch` == true means the callback receives the whole normalized questions
 * array (as a JSON string) instead of a single question; it should return a
 * JSON string `{"answers": {qid: raw}, "timed_out": bool}` or the same shape
 * as a dict. Legacy callbacks (batch==false) are looped one question at a time.
 */
typedef char *(*clarify_callback)(const char *question, const char *choices_json,
                                  bool multi_select, void *user_data);

/* ---- Pure helpers (exact name parity with tools/clarify_tool.py) ---- */

/* Coerce one choice into its user-facing display string (str | dict | list). */
char *clarify_flatten_choice(const char *choice_json);

/* Label the first choice as the agent's recommendation (idempotent). */
char **clarify_mark_recommended(char **choices, size_t count); /* returns new array */

/* Remove the recommendation label from a resolved answer. */
char *clarify_strip_recommended(const char *text);

/* Parse a multi-select response into a list of cleaned choice strings. */
/* Returns a NULL-terminated array of heap strings (caller frees each + array). */
char **clarify_parse_multi_select_response(const char *raw_response);

/*
 * Validate and normalize the `questions` batch parameter.
 * Returns a JSON string (the normalized list) on success, or an error JSON
 * string `{"error": ...}` on failure. Empty array -> returns the literal
 * string "null" (the caller falls through to the single-question path).
 */
char *clarify_normalize_questions(const char *questions_json);

/* Strip presentation from one locked answer (label, multi-select JSON). */
/* `entry_json` is one normalized question object. Returns heap JSON value. */
char *clarify_clean_batch_answer(const char *entry_json, const char *raw);

/* Assemble the batch result JSON from per-qid answers. */
char *clarify_batch_result(const char *normalized_json, const char *answers_json,
                           bool timed_out);

/* Dispatch a validated batch to the platform callback (or loop legacy). */
char *clarify_run_batch(const char *normalized_json, clarify_callback cb,
                        const char *title, bool cb_is_batch, void *user_data);

/*
 * clarify_tool — ask the user a question, optionally multiple-choice / batch.
 * Returns a JSON result string (the contract the agent consumes).
 *
 * `questions_json` may be NULL (single-question path). `callback` may be NULL
 * (returns an availability error). `choices_json` may be NULL (open-ended).
 */
char *clarify_tool(const char *question, const char *choices_json, bool multi_select,
                   const char *questions_json, clarify_callback cb, bool cb_is_batch,
                   void *user_data);

/* Clarify tool has no external requirements — always available. */
bool clarify_check_requirements(void);

/*
 * Capability probes mirroring the Python signature-introspection helpers.
 * In C the caller declares its callback's capability via `cb_is_batch`
 * (there is no signature introspection); these return that declared flag.
 */
bool clarify_callback_accepts_questions(bool cb_is_batch);
char *clarify_invoke_callback(const char *question, const char *choices_json,
                              bool multi_select, clarify_callback cb,
                              bool cb_is_batch, const char *normalized_json,
                              void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* CLARIFY_TOOL_H */
