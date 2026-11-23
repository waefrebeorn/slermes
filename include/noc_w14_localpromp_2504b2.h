#ifndef NOC_W14_LOCALPROMP_2504B2_H
#define NOC_W14_LOCALPROMP_2504B2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_localpromp_2504b2_with_prompt_progress @ agent/local_prompt_progress.py:with_prompt_progress
 * Returns a new object; extra_body is a copy with return_progress defaulted to true. */
struct json_t *w14_localpromp_2504b2_with_prompt_progress(const struct json_t *kwargs);

/* PoP: w14_localpromp_2504b2_prompt_progress @ agent/local_prompt_progress.py:prompt_progress
 * Returns the progress object for a progress-only chunk, or JSON null.
 * Chunk attributes are injected as object keys (model_extra/choices/prompt_progress). */
struct json_t *w14_localpromp_2504b2_prompt_progress(const struct json_t *chunk);

/* PoP: w14_localpromp_2504b2_prompt_progress_percent @ agent/local_prompt_progress.py:prompt_progress_percent
 * 0-100 integer as a JSON number, or JSON null without a usable positive total.
 * Python round() is banker's rounding (ties to even). */
struct json_t *w14_localpromp_2504b2_prompt_progress_percent(const struct json_t *progress);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_LOCALPROMP_2504B2_H */
