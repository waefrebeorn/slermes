#ifndef NOC_W22_QUERYREWRI_D659A2_H
#define NOC_W22_QUERYREWRI_D659A2_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/memory/query_rewrite.py (gate w22_queryrewri_d659a2).
 *
 * `rewrite_memory_query` is a model call wrapped around three pure steps: bound
 * the user message, pull the text out of the response, and normalize/reject
 * the candidate. The model call is the seam (`llm_raises`, and the response
 * itself as JSON); the three steps are ported whole, including the fences, the
 * label prefix, the control/whitespace squeeze and every rejection rule.
 *
 * Strings are UTF-8. Lengths and slices count characters, as Python's do; the
 * Unicode space set is the ASCII one plus \x1c-\x1f (fixtures stay in ASCII).
 * Every returned string is malloc'd; the caller frees it.
 */

/* PoP: w22_queryrewri_d659a2_bounded_user_message @ plugins/memory/query_rewrite.py:_bounded_user_message
 * `message.strip()`, or over 4000 characters: the first 3000 (right-trimmed),
 * the omission marker, and the last 900 (left-trimmed). */
char *w22_queryrewri_d659a2_bounded_user_message(const char *message);

/* PoP: w22_queryrewri_d659a2_extract_response_text @ plugins/memory/query_rewrite.py:_extract_response_text
 * `response.choices[0].message.content`: a string as-is, a list of parts joined
 * over their string `text` fields, anything else "". response:
 * {"choices": [{"message": {"content": <str|array|null>}}]}. */
char *w22_queryrewri_d659a2_extract_response_text(const json_t *response);

/* PoP: w22_queryrewri_d659a2_normalize_rewrite @ plugins/memory/query_rewrite.py:_normalize_rewrite
 * The candidate cleanup and the rejection rules; "" when the rewrite is not a
 * single grounded question, else the question with its trailing "?". */
char *w22_queryrewri_d659a2_normalize_rewrite(const char *text);

/* PoP: w22_queryrewri_d659a2_rewrite @ plugins/memory/query_rewrite.py:rewrite_memory_query
 * "" for an empty bounded message or a failed model call, else
 * `normalize_rewrite(extract_response_text(response))`. */
char *w22_queryrewri_d659a2_rewrite(const char *user_message, const json_t *response,
                                    int llm_raises);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_QUERYREWRI_D659A2_H */
