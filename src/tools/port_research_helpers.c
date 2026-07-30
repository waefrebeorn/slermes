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
#include <stdio.h>
#include <ctype.h>

#include "hash.h"

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

/* ============================================================
 * ResearchCache._make_key
 * ============================================================ */

/* PoP: research_cache_make_key @ tools/online_research.py:_make_key */
void research_cache_make_key(const char *query, const char *intent,
                             char out_key[33]) {
    if (!out_key) return;
    out_key[0] = '\0';
    const char *q = query ? query : "";
    const char *in = intent ? intent : "";
    /* f"{intent}:{query}" */
    size_t n = strlen(in) + 1 + strlen(q);
    char *buf = (char *)malloc(n + 1);
    if (!buf) return;
    snprintf(buf, n + 1, "%s:%s", in, q);
    char *hex = hash_sha256_hex((const unsigned char *)buf, n);
    free(buf);
    if (!hex) return;
    /* [:32] */
    memcpy(out_key, hex, 32);
    out_key[32] = '\0';
    free(hex);
}

/* ============================================================
 * OnlineResearcher._score_relevance
 * ============================================================ */

/* PoP: research_score_relevance @ tools/online_research.py:_score_relevance */
double research_score_relevance(const char *title, const char *snippet,
                                double base_score) {
    double score = base_score;
    const char *t = title ? title : "";
    const char *s = snippet ? snippet : "";
    /* text = f"{title} {snippet}".lower() */
    size_t n = strlen(t) + 1 + strlen(s);
    char *text = (char *)malloc(n + 1);
    if (!text) return score;
    snprintf(text, n + 1, "%s %s", t, s);
    for (size_t i = 0; text[i]; i++) text[i] = (char)tolower((unsigned char)text[i]);

    static const char *benchmark_terms[] = {
        "swe-bench", "terminal-bench", "livecodebench", "aa coding",
        "frontiermath", "aime", "gpqa", "hmmt", "mmlu", "ifeval", NULL
    };
    for (int i = 0; benchmark_terms[i]; i++)
        if (strstr(text, benchmark_terms[i])) score += 0.15;

    static const char *model_terms[] = {
        "nemotron", "kimi", "glm-5", "minimax", "qwen", "deepseek",
        "llama", "gemma", "phi", "mistral", "gpt-oss", NULL
    };
    for (int i = 0; model_terms[i]; i++)
        if (strstr(text, model_terms[i])) score += 0.1;

    static const char *recency[] = {
        "2025", "2026", "latest", "new", "updated", NULL
    };
    for (int i = 0; recency[i]; i++) {
        if (strstr(text, recency[i])) { score += 0.05; break; }
    }

    free(text);
    if (score > 1.0) score = 1.0;
    return score;
}

/* ============================================================
 * OnlineResearcher._extract_model_scores
 * ============================================================ */

/* PoP: research_extract_model_scores @ tools/online_research.py:_extract_model_scores */
research_model_score_t *research_extract_model_scores(const char **titles,
                                                      const char **snippets,
                                                      int result_count,
                                                      int *out_n) {
    if (out_n) *out_n = 0;
    if (result_count < 0) result_count = 0;

    /* model_id -> list of patterns (faithful to the Python dict) */
    struct model_pat { const char *model_id; const char *patterns[4]; };
    static const struct model_pat MODELS[] = {
        {"moonshotai/kimi-k2.6", {"kimi-k2.6", "kimi k2.6", "kimi k2", NULL}},
        {"z-ai/glm-5", {"glm-5", "glm 5", "z.ai glm", NULL}},
        {"minimaxai/minimax-m2.5", {"minimax-m2.5", "minimax m2.5", "minimax 2.5", NULL}},
        {"nvidia/nemotron-3-ultra-550b-a55b", {"nemotron-3-ultra", "nemotron 3 ultra", "550b", NULL}},
        {"nvidia/nemotron-4-340b-instruct", {"nemotron-4-340b", "nemotron 4 340b", NULL, NULL}},
        {"nvidia/llama-3.1-nemotron-ultra-253b-v1", {"nemotron-ultra-253b", "nemotron ultra 253b", NULL, NULL}},
        {"nvidia/nemotron-3-super-120b-a12b", {"nemotron-3-super", "nemotron 3 super", "120b", NULL}},
        {"qwen/qwen3.5-397b-a17b", {"qwen3.5-397b", "qwen 3.5 397b", NULL, NULL}},
        {"deepseek-ai/deepseek-v3.2", {"deepseek-v3.2", "deepseek v3.2", NULL, NULL}},
        {"openai/gpt-oss-120b", {"gpt-oss-120b", "gpt oss 120b", NULL, NULL}},
        {"meta/llama-3.1-405b-instruct", {"llama-3.1-405b", "llama 405b", NULL, NULL}},
    };
    const int NMODELS = (int)(sizeof(MODELS) / sizeof(MODELS[0]));

    research_model_score_t *out =
        (research_model_score_t *)calloc((size_t)NMODELS, sizeof(*out));
    if (!out) return NULL;
    /* index by model to keep max, matching dict semantics */
    double *best = (double *)calloc((size_t)NMODELS, sizeof(double));
    int *seen = (int *)calloc((size_t)NMODELS, sizeof(int));
    if (!best || !seen) { free(out); free(best); free(seen); return NULL; }

    for (int r = 0; r < result_count; r++) {
        const char *t = (titles && titles[r]) ? titles[r] : "";
        const char *s = (snippets && snippets[r]) ? snippets[r] : "";
        size_t n = strlen(t) + 1 + strlen(s);
        char *text = (char *)malloc(n + 1);
        if (!text) continue;
        snprintf(text, n + 1, "%s %s", t, s);
        for (size_t i = 0; text[i]; i++)
            text[i] = (char)tolower((unsigned char)text[i]);
        size_t textlen = strlen(text);

        for (int mi = 0; mi < NMODELS; mi++) {
            for (int pi = 0; MODELS[mi].patterns[pi]; pi++) {
                const char *pat = MODELS[mi].patterns[pi];
                char *hit = strstr(text, pat);
                if (!hit) continue;
                /* context window +-100 chars */
                size_t pos = (size_t)(hit - text);
                size_t cs = pos > 100 ? pos - 100 : 0;
                size_t ce = pos + strlen(pat) + 100;
                if (ce > textlen) ce = textlen;
                /* extract percentages like 58.6% within context */
                int found_pct = 0;
                for (size_t i = cs; i < ce; i++) {
                    if (isdigit((unsigned char)text[i])) {
                        size_t j = i;
                        char num[32]; size_t k = 0;
                        while (j < ce && k < sizeof(num) - 1 &&
                               (isdigit((unsigned char)text[j]) || text[j] == '.'))
                            num[k++] = text[j++];
                        num[k] = '\0';
                        if (j < ce && text[j] == '%') {
                            double val = atof(num);
                            if (val >= 0 && val <= 100) {
                                double nv = val / 100.0;
                                if (!seen[mi] || nv > best[mi]) best[mi] = nv;
                                seen[mi] = 1;
                                found_pct = 1;
                            }
                            i = j; /* skip past */
                        } else {
                            i = j - 1;
                        }
                    }
                }
                if (!found_pct) {
                    double nv = 0.5;
                    if (!seen[mi] || nv > best[mi]) best[mi] = nv;
                    seen[mi] = 1;
                }
                break; /* Python breaks after first matching pattern */
            }
        }
        free(text);
    }

    int cnt = 0;
    for (int mi = 0; mi < NMODELS; mi++) {
        if (seen[mi]) {
            strncpy(out[cnt].model_id, MODELS[mi].model_id,
                    sizeof(out[cnt].model_id) - 1);
            out[cnt].model_id[sizeof(out[cnt].model_id) - 1] = '\0';
            out[cnt].score = best[mi];
            cnt++;
        }
    }
    free(best);
    free(seen);
    if (out_n) *out_n = cnt;
    return out;
}
