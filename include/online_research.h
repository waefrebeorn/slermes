/*
 * online_research.h — Online Research Module for MoA (Hermes C/slermes)
 * 
 * Provides web search capabilities for dynamic model selection and context enrichment.
 * - Multi-source web search (DuckDuckGo HTML, Brave API, Google CSE)
 * - Research quality scoring
 * - Dynamic totem pole reordering based on benchmark updates
 * - In-memory caching with TTL
 * - Integration with MoA pipeline
 */

#ifndef HERMES_ONLINE_RESEARCH_H
#define HERMES_ONLINE_RESEARCH_H

#include "hermes_core_types.h"
#include "hermes_json.h"
#include <time.h>

/* ─── MoA Reference Model Type (needed for reordering) ────────────── */

typedef struct {
    const char *provider;
    const char *model;
    float temperature;
    int max_tokens;
    const char *reasoning_effort;
    const char *role;
    int benchmark_tier;
} moa_ref_model_t;

typedef struct {
    char *title;
    char *url;
    char *snippet;
    char *source;
    float relevance_score;
    time_t timestamp;
} research_result_t;

typedef struct {
    char *query;
    char *intent;  // "benchmark_update", "model_comparison", "technical_fact", "general"
    research_result_t *findings;
    int num_findings;
    float confidence;
    time_t timestamp;
    char *cache_key;
    
    // Model scores extracted from research: model_id -> score (0-1)
    char **model_ids;
    float *model_scores;
    int num_model_scores;
} research_summary_t;

/* ─── Public API ─────────────────────────────────────────────────── */

/**
 * Conduct online research for a MoA prompt.
 * Returns research summary with findings and model scores.
 * Caller must free with moa_research_free().
 */
research_summary_t *moa_research_for_prompt(const char *user_prompt);

/**
 * Free a research summary and all its contents.
 */
void moa_research_free(research_summary_t *research);

/**
 * Apply research-based reordering to reference models.
 * Reorders the refs array in place based on model scores from research.
 * 
 * @param refs Pointer to array of reference models (will be reordered)
 * @param count Pointer to count of reference models
 * @param research Research summary with model scores
 */
void moa_apply_research_to_refs(moa_ref_model_t **refs, int *count, research_summary_t *research);

/* ─── Python module-level API (tools/mixture_of_agents_tool.py) ────────
 * Wrappers mapping the Python module names onto the real MoA engine. */
int moa_provider_health_is_healthy(const char *provider_name);
void moa_provider_health_record_success(const char *provider_name);
void moa_provider_health_record_failure(const char *provider_name);
char *moa_provider_health_get_summary(void);
char *moa_http_client_call_model(const moa_ref_model_t *ref,
                                 const char *system_prompt, const char *user_prompt);
char *moa_http_client_extract_content(const char *data_json);
char **moa_query_all_references(const moa_ref_model_t *refs, int ref_count,
                                const char *system_prompt, const char *user_prompt,
                                int *out_pairs);
char *moa_mixture_of_agents_math(const char *user_prompt);
void moa_http_client_enter(void);
void moa_http_client_exit(void);

/* Main engine entry (tools/mixture_of_agents.c) — returns JSON string (caller frees). */
char *handle_mixture_of_agents(const char *args_json, const char *task_id);

/* ─── Cache Configuration ────────────────────────────────────────── */

/**
 * Set research cache TTL in seconds (default 3600).
 */
void moa_research_set_cache_ttl(int ttl_seconds);
/* Researcher session lifecycle (port of online_research.py global slot). */
void online_research_close_session(void);
void online_research_open_session(void);
bool online_research_session_active(void);

/**
 * Clear expired cache entries.
 */
void moa_research_clear_expired_cache();

#endif /* HERMES_ONLINE_RESEARCH_H */