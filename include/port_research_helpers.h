/*
 * port_research_helpers.h — Faithful C11 ports of module-level pure helpers
 * from Python tools/online_research.py (REAL_GAP set in the parity
 * battleground).
 */

#ifndef PORT_RESEARCH_HELPERS_H
#define PORT_RESEARCH_HELPERS_H

#include <stddef.h>

/* reorder_totem_pole_by_research(current_order, order_count,
 *   research_models, research_scores, scores_count, weight, out_count)
 * -> malloc'd array of strdup'd model ids (NULL-terminated), caller frees
 * each + array. Returns NULL on invalid input. */
char **research_reorder_totem_pole_by_research(const char **current_order,
                                               int order_count,
                                               const char **research_models,
                                               const double *research_scores,
                                               int scores_count,
                                               double weight,
                                               int *out_count);

/* ResearchCache._make_key(query, intent) ->
 *   sha256(f"{intent}:{query}").hexdigest()[:32]
 * Writes a 33-byte (32 hex chars + NUL) key into out_key (must hold >=33). */
void research_cache_make_key(const char *query, const char *intent,
                             char out_key[33]);

/* OnlineResearcher._score_relevance(title, snippet, base_score) ->
 *   base_score boosted by benchmark terms (+0.15 each), model names (+0.1
 *   each), recency indicators (+0.05 once), clamped to <= 1.0. Faithful to
 *   the Python term lists. query/intent are unused by the Python body but
 *   kept for signature parity. */
double research_score_relevance(const char *title, const char *snippet,
                                double base_score);

/* One extracted model score. */
typedef struct {
    char model_id[96];
    double score;
} research_model_score_t;

/* OnlineResearcher._extract_model_scores(results, query) ->
 *   dict[model_id -> score]. Given N results (title+snippet arrays), scans
 *   for each known model pattern; when found, extracts percentages in a
 *   +-100 char context window and normalizes to 0..1 (max wins), else 0.5.
 *   Returns a malloc'd array (caller frees); *out_n set to entry count. */
research_model_score_t *research_extract_model_scores(const char **titles,
                                                      const char **snippets,
                                                      int result_count,
                                                      int *out_n);

#endif /* PORT_RESEARCH_HELPERS_H */
