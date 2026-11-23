#ifndef NOC_W10_SLASHFUZZY_F16515_H
#define NOC_W10_SLASHFUZZY_F16515_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_slashfuzzy_f16515_tokenize_search_text @ tui_gateway/slash_fuzzy.py:tokenize_search_text */
struct json_t *w10_slashfuzzy_f16515_tokenize_search_text(const char *value);
/* PoP: w10_slashfuzzy_f16515_normalize_slash_search_query @ tui_gateway/slash_fuzzy.py:normalize_slash_search_query
 * Returns a malloc'd string; caller frees. */
char *w10_slashfuzzy_f16515_normalize_slash_search_query(const char *query);
/* PoP: w10_slashfuzzy_f16515_score_slash_completion_item @ tui_gateway/slash_fuzzy.py:score_slash_completion_item
 * Returns INFINITY for a miss (mirrors math.inf). */
double w10_slashfuzzy_f16515_score_slash_completion_item(const struct json_t *item, const char *query);
/* PoP: w10_slashfuzzy_f16515_fuzzy_rank_slash_items @ tui_gateway/slash_fuzzy.py:fuzzy_rank_slash_items
 * Returns the ranked list (first element of the Python tuple); caller frees. */
struct json_t *w10_slashfuzzy_f16515_fuzzy_rank_slash_items(const struct json_t *items,
                                                            const struct json_t *catalog,
                                                            const char *query);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SLASHFUZZY_F16515_H */
