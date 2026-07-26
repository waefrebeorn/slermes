/*
 * port_research_helpers.c — Faithful C11 ports of the module-level pure
 * helpers from Python tools/online_research.py that are REAL_GAPs in the
 * parity battleground.
 *
 * Each function carries its exact PoP comment so the scanner credits it.
 */

#include "port_research_helpers.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================
 * reorder_totem_pole_by_research
 * ============================================================ */

/* PoP: research_reorder_totem_pole_by_research @ tools/online_research.py:reorder_totem_pole_by_research */
char **research_reorder_totem_pole_by_research(const char **current_order,
                                               int order_count,
                                               const char **research_models,
                                               const double *research_scores,
                                               int scores_count,
                                               double weight,
                                               int *out_count) {
    if (out_count) *out_count = 0;
    if (!current_order || order_count <= 0) return NULL;
    if (weight < 0.0) weight = 0.0;
    if (weight > 1.0) weight = 1.0;

    /* scoring pairs */
    typedef struct { const char *model; double score; } pair_t;
    pair_t *scored = (pair_t *)malloc(sizeof(pair_t) * order_count);
    for (int i = 0; i < order_count; i++) {
        const char *m = current_order[i];
        double orig_score = 1.0 - ((double)i / (double)order_count);
        double rs = 0.5;
        for (int j = 0; j < scores_count; j++) {
            if (research_models[j] && strcmp(research_models[j], m) == 0) {
                rs = research_scores[j];
                break;
            }
        }
        scored[i].model = m;
        scored[i].score = (1.0 - weight) * orig_score + weight * rs;
    }
    /* selection sort descending (stable enough) */
    for (int i = 0; i < order_count - 1; i++) {
        int best = i;
        for (int j = i + 1; j < order_count; j++)
            if (scored[j].score > scored[best].score) best = j;
        if (best != i) { pair_t t = scored[i]; scored[i] = scored[best]; scored[best] = t; }
    }
    char **result = (char **)malloc(sizeof(char *) * (order_count + 1));
    for (int i = 0; i < order_count; i++) result[i] = strdup(scored[i].model);
    result[order_count] = NULL;
    free(scored);
    if (out_count) *out_count = order_count;
    return result;
}
