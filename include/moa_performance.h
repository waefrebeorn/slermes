/*
 * moa_performance.h — MoA Performance & Consistency Module for Hermes C (slermes)
 * 
 * Provides:
 * - Thread pool reuse for parallel model queries
 * - HTTP connection pooling via libhttp
 * - SQLite-backed response cache (project-aware, LRU eviction)
 * - SQLite-backed research cache (project-aware, TTL expiry)
 * - Project context management (persistent, auto-detected from CWD)
 * - Consistent model selection per project/mode
 * - Session history tracking
 */

#ifndef HERMES_MOA_PERFORMANCE_H
#define HERMES_MOA_PERFORMANCE_H

#include "hermes_core_types.h"
#include "online_research.h"
#include <time.h>

/* ─── Forward Declarations ────────────────────────────────────────── */

typedef struct moa_project_context_s moa_project_context_t;
typedef struct moa_cached_response_s moa_cached_response_t;
typedef struct moa_cached_research_s moa_cached_research_t;

/* ─── Project Context ─────────────────────────────────────────────── */

struct moa_project_context_s {
    char project_id[17];          // 16-char hex + null
    char project_name[256];
    char working_dir[512];
    char goal[1024];
    char preferred_models_json[4096];  // JSON: {"mode": "provider:model", ...}
    double created_at;
    double updated_at;
};

/* Project context manager */
moa_project_context_t *moa_project_get_or_create(const char *working_dir);
void moa_project_update(moa_project_context_t *ctx);
void moa_project_record_session(const char *project_id, const char *mode, const char *prompt,
                                 const char *result_summary, const char **models_used, 
                                 int models_count, double duration);
const char *moa_project_get_preferred_model(moa_project_context_t *ctx, const char *mode);
void moa_project_set_preferred_model(moa_project_context_t *ctx, const char *mode, const char *model);

/* ─── Response Cache ──────────────────────────────────────────────── */

struct moa_cached_response_s {
    char *response;
    char *models_used_json;
    double created_at;
    int hit_count;
};

moa_cached_response_t *moa_response_cache_get(const char *prompt, const char *mode, const char *project_id);
void moa_response_cache_set(const char *prompt, const char *mode, const char *response,
                             const char **models_used, int models_count, const char *project_id);
void moa_cached_response_free(moa_cached_response_t *resp);

/* ─── Research Cache ──────────────────────────────────────────────── */

struct moa_cached_research_s {
    char *findings_json;
    char *model_scores_json;
    double confidence;
    double created_at;
};

moa_cached_research_t *moa_research_cache_get(const char *query, const char *intent, const char *project_id);
void moa_research_cache_set(const char *query, const char *intent,
                             const char *findings_json, const char *model_scores_json,
                             double confidence, const char *project_id);
void moa_cached_research_free(moa_cached_research_t *research);

/* ─── Consistent Model Selector ───────────────────────────────────── */

const moa_ref_model_t *moa_model_selector_get_preferred(const moa_ref_model_t *refs, int ref_count,
                                                         moa_project_context_t *project, const char *mode);
void moa_model_selector_record_success(moa_project_context_t *project, const char *mode, const moa_ref_model_t *model);

/* ─── Thread Pool ─────────────────────────────────────────────────── */

int moa_thread_pool_submit(void *(*fn)(void *), void *arg, void **result_out);
void moa_thread_pool_shutdown(void);

/* ─── HTTP Connection Pool (uses libhttp internally) ──────────────── */

typedef struct moa_http_pool_s moa_http_pool_t;

moa_http_pool_t *moa_http_pool_get(void);
void moa_http_pool_shutdown(void);

/* ─── Shutdown ────────────────────────────────────────────────────── */

void moa_performance_shutdown(void);

#endif /* HERMES_MOA_PERFORMANCE_H */