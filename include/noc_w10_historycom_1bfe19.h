#ifndef NOC_W10_HISTORYCOM_1BFE19_H
#define NOC_W10_HISTORYCOM_1BFE19_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_historycom_1bfe19_commentary_items @ agent/history_commentary.py:commentary_items
 * Assistant message items phased "commentary" -> list of stripped output_text strings; caller frees. */
struct json_t *w10_historycom_1bfe19_commentary_items(const struct json_t *message);

/* PoP: w10_historycom_1bfe19_final_items @ agent/history_commentary.py:_final_items
 * Assistant message items phased final/final_answer/unphased -> list of strings; caller frees. */
struct json_t *w10_historycom_1bfe19_final_items(const struct json_t *message);

/* PoP: w10_historycom_1bfe19_without_flattened_commentary @ agent/history_commentary.py:_without_flattened_commentary
 * Omit exact whole-line public segments from a display copy of reasoning; caller frees the string. */
char *w10_historycom_1bfe19_without_flattened_commentary(const char *reasoning, const struct json_t *commentary);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_HISTORYCOM_1BFE19_H */
