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

#endif /* PORT_RESEARCH_HELPERS_H */
