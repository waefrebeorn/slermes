/*
 * online_research.c — Online Research Module for MoA (Hermes C/slermes)
 * 
 * Provides web search capabilities for dynamic model selection and context enrichment.
 * - Multi-source web search (DuckDuckGo HTML, Brave API, Google CSE)
 * - Research quality scoring
 * - Dynamic totem pole reordering based on benchmark updates
 * - In-memory caching with TTL
 * - Integration with MoA pipeline
 */

#include "online_research.h"
#include "hermes_http.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "hermes_core_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <regex.h>
#include <math.h>
#include <ctype.h>

/* ─── Cache ───────────────────────────────────────────────────────── */

static int g_cache_ttl = 3600;  // 1 hour default

typedef struct {
    research_summary_t **entries;
    int count;
    int capacity;
    pthread_mutex_t mutex;
    int initialized;
} cache_t;

static cache_t g_cache = {0};

static void cache_init() {
    if (g_cache.initialized) return;
    g_cache.capacity = 64;
    g_cache.entries = calloc(g_cache.capacity, sizeof(research_summary_t*));
    pthread_mutex_init(&g_cache.mutex, NULL);
    g_cache.initialized = 1;
}

static char *make_cache_key(const char *query, const char *intent) {
    size_t len = strlen(query) + strlen(intent) + 2;
    char *combined = malloc(len);
    snprintf(combined, len, "%s|%s", intent, query);
    
    // Simple hash
    unsigned long hash = 5381;
    for (size_t i = 0; combined[i]; i++) {
        hash = ((hash << 5) + hash) + combined[i];
    }
    free(combined);
    
    char *key = malloc(33);
    snprintf(key, 33, "%lx", hash);
    return key;
}

static research_summary_t *cache_get(const char *query, const char *intent) {
    cache_init();
    char *key = make_cache_key(query, intent);
    research_summary_t *result = NULL;
    
    pthread_mutex_lock(&g_cache.mutex);
    for (int i = 0; i < g_cache.count; i++) {
        if (g_cache.entries[i] && strcmp(g_cache.entries[i]->cache_key, key) == 0) {
            if (time(NULL) - g_cache.entries[i]->timestamp < g_cache_ttl) {
                result = g_cache.entries[i];
            } else {
                // Expired - remove
                free(g_cache.entries[i]->cache_key);
                for (int j = 0; j < g_cache.entries[i]->num_findings; j++) {
                    free(g_cache.entries[i]->findings[j].title);
                    free(g_cache.entries[i]->findings[j].url);
                    free(g_cache.entries[i]->findings[j].snippet);
                    free(g_cache.entries[i]->findings[j].source);
                }
                free(g_cache.entries[i]->findings);
                for (int j = 0; j < g_cache.entries[i]->num_model_scores; j++) {
                    free(g_cache.entries[i]->model_ids[j]);
                }
                free(g_cache.entries[i]->model_ids);
                free(g_cache.entries[i]->model_scores);
                free(g_cache.entries[i]->query);
                free(g_cache.entries[i]->intent);
                free(g_cache.entries[i]);
                g_cache.entries[i] = g_cache.entries[--g_cache.count];
                g_cache.entries[g_cache.count] = NULL;
            }
            break;
        }
    }
    pthread_mutex_unlock(&g_cache.mutex);
    
    free(key);
    return result;
}

static void cache_set(research_summary_t *summary) {
    cache_init();
    
    pthread_mutex_lock(&g_cache.mutex);
    if (g_cache.count >= g_cache.capacity) {
        g_cache.capacity *= 2;
        g_cache.entries = realloc(g_cache.entries, g_cache.capacity * sizeof(research_summary_t*));
    }
    g_cache.entries[g_cache.count++] = summary;
    pthread_mutex_unlock(&g_cache.mutex);
}

void moa_research_set_cache_ttl(int ttl_seconds) {
    g_cache_ttl = ttl_seconds > 0 ? ttl_seconds : 3600;
}

/* Researcher session slot (Python module-global _researcher). The C
 * researcher is synchronous (moa_research_for_prompt), so the slot
 * tracks active/cleared state for lifecycle parity. */
static bool g_researcher_active = false;

/* Port of tools/online_research.py:close_researcher — teardown + clear. */
void online_research_close_session(void) {
    g_researcher_active = false;
}

/* Port of tools/online_research.py:get_researcher — mark session active. */
void online_research_open_session(void) {
    g_researcher_active = true;
}

bool online_research_session_active(void) {
    return g_researcher_active;
}

void moa_research_clear_expired_cache() {
    cache_init();
    time_t now = time(NULL);
    
    pthread_mutex_lock(&g_cache.mutex);
    for (int i = 0; i < g_cache.count; ) {
        if (now - g_cache.entries[i]->timestamp >= g_cache_ttl) {
            // Free expired
            research_summary_t *e = g_cache.entries[i];
            free(e->cache_key);
            for (int j = 0; j < e->num_findings; j++) {
                free(e->findings[j].title);
                free(e->findings[j].url);
                free(e->findings[j].snippet);
                free(e->findings[j].source);
            }
            free(e->findings);
            for (int j = 0; j < e->num_model_scores; j++) {
                free(e->model_ids[j]);
            }
            free(e->model_ids);
            free(e->model_scores);
            free(e->query);
            free(e->intent);
            free(e);
            g_cache.entries[i] = g_cache.entries[--g_cache.count];
            g_cache.entries[g_cache.count] = NULL;
        } else {
            i++;
        }
    }
    pthread_mutex_unlock(&g_cache.mutex);
}

/* ─── HTTP Search Functions ──────────────────────────────────────── */

static char *url_encode(const char *str) {
    // Simple URL encoding for query parameters
    static const char *hex = "0123456789ABCDEF";
    size_t len = strlen(str);
    char *out = malloc(len * 3 + 1);
    char *p = out;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = str[i];
        if (c == ' ') {
            *p++ = '+';
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
                   (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            *p++ = c;
        } else {
            *p++ = '%';
            *p++ = hex[c >> 4];
            *p++ = hex[c & 0xF];
        }
    }
    *p = '\0';
    return out;
}

static research_result_t *parse_duckduckgo_html(const char *html, int *out_count, int max_results) {
    research_result_t *results = calloc(max_results, sizeof(research_result_t));
    int count = 0;
    
    regex_t regex;
    if (regcomp(&regex, "class=\"result__snippet\">([^<]+)</a>.*?class=\"result__url\">([^<]+)</a>", REG_EXTENDED) != 0) {
        free(results);
        return NULL;
    }
    
    regmatch_t matches[3];
    const char *cursor = html;
    while (count < max_results && regexec(&regex, cursor, 3, matches, 0) == 0) {
        int snippet_len = matches[1].rm_eo - matches[1].rm_so;
        results[count].snippet = strndup(cursor + matches[1].rm_so, snippet_len);
        results[count].title = strndup(results[count].snippet, 80);
        
        int url_len = matches[2].rm_eo - matches[2].rm_so;
        results[count].url = strndup(cursor + matches[2].rm_so, url_len);
        
        results[count].source = strdup("duckduckgo");
        results[count].relevance_score = 0.7f;
        results[count].timestamp = time(NULL);
        
        count++;
        cursor += matches[0].rm_eo;
    }
    
    regfree(&regex);
    *out_count = count;
    return results;
}

/* PoP: search_duckduckgo @ tools.online_research.py:search_duckduckgo */
static research_result_t *search_duckduckgo(const char *query, int num_results) {
    char *encoded = url_encode(query);
    char url[1024];
    snprintf(url, sizeof(url), "https://html.duckduckgo.com/html/?q=%s", encoded);
    free(encoded);
    
    http_t *http = http_new(30);
    if (!http) return NULL;
    
    http_resp_t *resp = http_request(http, HTTP_GET, url, "User-Agent: Hermes-MoA/1.0\nAccept: text/html\n", NULL, 0);
    http_free(http);
    
    if (!resp || resp->status != 200 || !resp->body) {
        if (resp) http_resp_free(resp);
        return NULL;
    }
    
    int count = 0;
    research_result_t *results = parse_duckduckgo_html(resp->body, &count, num_results);
    http_resp_free(resp);
    return results;
}

static research_result_t *search_brave(const char *query, int num_results) {
    const char *api_key = getenv("BRAVE_API_KEY");
    if (!api_key) return NULL;
    
    char *encoded = url_encode(query);
    char url[512];
    snprintf(url, sizeof(url), "https://api.search.brave.com/res/v1/web/search?q=%s&count=%d", encoded, num_results);
    free(encoded);
    
    http_t *http = http_new(30);
    if (!http) return NULL;
    
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s\nAccept: application/json\n", api_key);
    
    http_resp_t *resp = http_request(http, HTTP_GET, url, auth_header, NULL, 0);
    http_free(http);
    
    if (!resp || resp->status != 200 || !resp->body) {
        if (resp) http_resp_free(resp);
        return NULL;
    }
    
    json_t *data = json_parse(resp->body, NULL);
    http_resp_free(resp);
    
    if (!data) return NULL;
    
    json_t *web = json_obj_get(data, "web");
    json_t *results_arr = web ? json_obj_get(web, "results") : NULL;
    
    if (!results_arr || results_arr->type != JSON_ARRAY) {
        json_free(data);
        return NULL;
    }
    
    int count = results_arr->c.count < num_results ? results_arr->c.count : num_results;
    research_result_t *results = calloc(count, sizeof(research_result_t));
    
    for (int i = 0; i < count; i++) {
        json_t *item = json_get(results_arr, i);
        results[i].title = strdup(json_get_str(item, "title", ""));
        results[i].url = strdup(json_get_str(item, "url", ""));
        results[i].snippet = strdup(json_get_str(item, "description", ""));
        results[i].source = strdup("brave");
        results[i].relevance_score = 0.85f;
        results[i].timestamp = time(NULL);
    }
    
    json_free(data);
    return results;
}

static research_result_t *search_google_cse(const char *query, int num_results) {
    const char *api_key = getenv("GOOGLE_CSE_KEY");
    const char *cse_id = getenv("GOOGLE_CSE_ID");
    if (!api_key || !cse_id) return NULL;
    
    char *encoded = url_encode(query);
    char url[512];
    snprintf(url, sizeof(url), "https://www.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%d", 
             api_key, cse_id, encoded, num_results);
    free(encoded);
    
    http_t *http = http_new(30);
    if (!http) return NULL;
    
    http_resp_t *resp = http_request(http, HTTP_GET, url, "Accept: application/json\n", NULL, 0);
    http_free(http);
    
    if (!resp || resp->status != 200 || !resp->body) {
        if (resp) http_resp_free(resp);
        return NULL;
    }
    
    json_t *data = json_parse(resp->body, NULL);
    http_resp_free(resp);
    
    if (!data) return NULL;
    
    json_t *items = json_obj_get(data, "items");
    if (!items || items->type != JSON_ARRAY) {
        json_free(data);
        return NULL;
    }
    
    int count = items->c.count < num_results ? items->c.count : num_results;
    research_result_t *results = calloc(count, sizeof(research_result_t));
    
    for (int i = 0; i < count; i++) {
        json_t *item = json_get(items, i);
        results[i].title = strdup(json_get_str(item, "title", ""));
        results[i].url = strdup(json_get_str(item, "link", ""));
        results[i].snippet = strdup(json_get_str(item, "snippet", ""));
        results[i].source = strdup("google_cse");
        results[i].relevance_score = 0.9f;
        results[i].timestamp = time(NULL);
    }
    
    json_free(data);
    return results;
}

/* ─── Relevance Scoring ──────────────────────────────────────────── */

static float score_relevance(const research_result_t *result, const char *query, const char *intent) {
    float score = result->relevance_score;
    size_t text_len = strlen(result->title) + strlen(result->snippet) + 1;
    char *text = malloc(text_len);
    snprintf(text, text_len, "%s %s", result->title, result->snippet);
    
    for (char *p = text; *p; p++) *p = tolower(*p);
    char *query_lower = strdup(query);
    for (char *p = query_lower; *p; p++) *p = tolower(*p);
    
    // Benchmark terms
    const char *benchmark_terms[] = {
        "swe-bench", "terminal-bench", "livecodebench", "aa coding",
        "frontiermath", "aime", "gpqa", "hmmt", "mmlu", "ifeval",
        "swe-pro", "swe-v", "browsecomp", "bfcl", "ruler"
    };
    for (int i = 0; i < sizeof(benchmark_terms)/sizeof(char*); i++) {
        if (strstr(text, benchmark_terms[i])) score += 0.15f;
    }
    
    // Model name terms
    const char *model_terms[] = {
        "nemotron", "kimi", "glm-5", "minimax", "qwen", "deepseek", 
        "llama", "gemma", "phi", "mistral", "gpt-oss"
    };
    for (int i = 0; i < sizeof(model_terms)/sizeof(char*); i++) {
        if (strstr(text, model_terms[i])) score += 0.1f;
    }
    
    // Recency boost
    if (strstr(text, "2025") || strstr(text, "2026") || strstr(text, "latest") || strstr(text, "new")) {
        score += 0.05f;
    }
    
    free(text);
    free(query_lower);
    
    return fminf(1.0f, score);
}

/* ─── Model Score Extraction ─────────────────────────────────────── */

static void extract_model_scores(const research_result_t *results, int count,
                                 char ***out_model_ids, float **out_scores, int *out_num) {
    const char *model_patterns[][2] = {
        {"nvidia_nim:moonshotai/kimi-k2.6", "kimi-k2.6"},
        {"nvidia_nim:z-ai/glm-5", "glm-5"},
        {"nvidia_nim:minimaxai/minimax-m2.5", "minimax-m2.5"},
        {"nvidia_nim:nvidia/nemotron-3-ultra-550b-a55b", "nemotron-3-ultra"},
        {"nvidia_nim:nvidia/nemotron-4-340b-instruct", "nemotron-4-340b"},
        {"nvidia_nim:nvidia/llama-3.1-nemotron-ultra-253b-v1", "nemotron-ultra-253b"},
        {"nvidia_nim:nvidia/nemotron-3-super-120b-a12b", "nemotron-3-super"},
        {"nvidia_nim:qwen/qwen3.5-397b-a17b", "qwen3.5-397b"},
        {"nvidia_nim:deepseek-ai/deepseek-v3.2", "deepseek-v3.2"},
        {"nvidia_nim:openai/gpt-oss-120b", "gpt-oss-120b"},
        {"nvidia_nim:meta/llama-3.1-405b-instruct", "llama-405b"},
    };
    int num_patterns = sizeof(model_patterns) / sizeof(model_patterns[0]);
    
    *out_model_ids = calloc(num_patterns, sizeof(char*));
    *out_scores = calloc(num_patterns, sizeof(float));
    *out_num = 0;
    
    for (int i = 0; i < count; i++) {
        size_t text_len = strlen(results[i].title) + strlen(results[i].snippet) + 1;
        char *text = malloc(text_len);
        snprintf(text, text_len, "%s %s", results[i].title, results[i].snippet);
        for (char *p = text; *p; p++) *p = tolower(*p);
        
        for (int m = 0; m < num_patterns; m++) {
            char *pattern = strdup(model_patterns[m][1]);
            for (char *p = pattern; *p; p++) *p = tolower(*p);
            
            if (strstr(text, pattern)) {
                float best_score = 0.5f;  // Default if mentioned
                
                // Look for percentage scores
                regex_t pct_regex;
                if (regcomp(&pct_regex, "([0-9]+\\.?[0-9]*)%", REG_EXTENDED) == 0) {
                    regmatch_t pct_match[2];
                    if (regexec(&pct_regex, text, 2, pct_match, 0) == 0) {
                        char *pct_str = strndup(text + pct_match[1].rm_so, 
                                               pct_match[1].rm_eo - pct_match[1].rm_so);
                        float val = atof(pct_str);
                        if (val >= 0 && val <= 100) best_score = fmaxf(best_score, val / 100.0f);
                        free(pct_str);
                    }
                    regfree(&pct_regex);
                }
                
                // Update or add
                int found = 0;
                for (int k = 0; k < *out_num; k++) {
                    if (strcmp((*out_model_ids)[k], model_patterns[m][0]) == 0) {
                        (*out_scores)[k] = fmaxf((*out_scores)[k], best_score);
                        found = 1;
                        break;
                    }
                }
                if (!found && *out_num < num_patterns) {
                    (*out_model_ids)[*out_num] = strdup(model_patterns[m][0]);
                    (*out_scores)[*out_num] = best_score;
                    (*out_num)++;
                }
            }
            free(pattern);
        }
        free(text);
    }
}

/* ─── Public API ─────────────────────────────────────────────────── */

research_summary_t *moa_research_for_prompt(const char *user_prompt) {
    if (!user_prompt) return NULL;
    
    // Build research query focused on benchmarks
    char query[512];
    snprintf(query, sizeof(query), "%s AI model benchmark SWE-Bench Terminal-Bench LiveCodeBench 2025 2026", user_prompt);
    
    // Check cache first
    research_summary_t *cached = cache_get(query, "benchmark_update");
    if (cached) {
        hermes_log(LOG_INFO, "moa", "Research cache hit for: %.60s", query);
        return cached;
    }
    
    hermes_log(LOG_INFO, "moa", "Researching: %.80s", query);
    
    // Collect from all sources
    research_result_t *all_results = NULL;
    int total_results = 0;
    int capacity = 30;
    all_results = calloc(capacity, sizeof(research_result_t));
    
    // DuckDuckGo (always available)
    research_result_t *ddg_results = search_duckduckgo(query, 10);
    if (ddg_results) {
        for (int i = 0; i < 10 && ddg_results[i].url; i++) {
            int dup = 0;
            for (int j = 0; j < total_results; j++) {
                if (strcmp(all_results[j].url, ddg_results[i].url) == 0) {
                    dup = 1;
                    if (ddg_results[i].relevance_score > all_results[j].relevance_score) {
                        all_results[j].relevance_score = ddg_results[i].relevance_score;
                    }
                    break;
                }
            }
            if (!dup && total_results < capacity) {
                all_results[total_results++] = ddg_results[i];
            } else {
                free(ddg_results[i].title);
                free(ddg_results[i].url);
                free(ddg_results[i].snippet);
                free(ddg_results[i].source);
            }
        }
        free(ddg_results);
    }
    
    // Brave (if API key available)
    int brave_count = 0;
    research_result_t *brave_results = search_brave(query, 10);
    if (brave_results) {
        for (int i = 0; i < 10 && brave_results[i].url; i++) {
            int dup = 0;
            for (int j = 0; j < total_results; j++) {
                if (strcmp(all_results[j].url, brave_results[i].url) == 0) {
                    dup = 1;
                    if (brave_results[i].relevance_score > all_results[j].relevance_score) {
                        all_results[j].relevance_score = brave_results[i].relevance_score;
                    }
                    break;
                }
            }
            if (!dup && total_results < capacity) {
                all_results[total_results++] = brave_results[i];
            } else {
                free(brave_results[i].title);
                free(brave_results[i].url);
                free(brave_results[i].snippet);
                free(brave_results[i].source);
            }
        }
        free(brave_results);
    }
    
    // Google CSE (if API key available)
    int google_count = 0;
    research_result_t *google_results = search_google_cse(query, 10);
    if (google_results) {
        for (int i = 0; i < 10 && google_results[i].url; i++) {
            int dup = 0;
            for (int j = 0; j < total_results; j++) {
                if (strcmp(all_results[j].url, google_results[i].url) == 0) {
                    dup = 1;
                    if (google_results[i].relevance_score > all_results[j].relevance_score) {
                        all_results[j].relevance_score = google_results[i].relevance_score;
                    }
                    break;
                }
            }
            if (!dup && total_results < capacity) {
                all_results[total_results++] = google_results[i];
            } else {
                free(google_results[i].title);
                free(google_results[i].url);
                free(google_results[i].snippet);
                free(google_results[i].source);
            }
        }
        free(google_results);
    }
    
    // Score all results
    for (int i = 0; i < total_results; i++) {
        all_results[i].relevance_score = score_relevance(&all_results[i], query, "benchmark_update");
    }
    
    // Sort by relevance (simple bubble sort, small N)
    for (int i = 0; i < total_results - 1; i++) {
        for (int j = 0; j < total_results - i - 1; j++) {
            if (all_results[j].relevance_score < all_results[j+1].relevance_score) {
                research_result_t tmp = all_results[j];
                all_results[j] = all_results[j+1];
                all_results[j+1] = tmp;
            }
        }
    }
    
    // Limit to 15 results
    if (total_results > 15) {
        for (int i = 15; i < total_results; i++) {
            free(all_results[i].title);
            free(all_results[i].url);
            free(all_results[i].snippet);
            free(all_results[i].source);
        }
        total_results = 15;
    }
    
    // Extract model scores
    char **model_ids = NULL;
    float *model_scores = NULL;
    int num_model_scores = 0;
    extract_model_scores(all_results, total_results, &model_ids, &model_scores, &num_model_scores);
    
    // Create summary
    research_summary_t *summary = calloc(1, sizeof(research_summary_t));
    summary->query = strdup(query);
    summary->intent = strdup("benchmark_update");
    summary->findings = all_results;
    summary->num_findings = total_results;
    summary->model_ids = model_ids;
    summary->model_scores = model_scores;
    summary->num_model_scores = num_model_scores;
    summary->confidence = fminf(1.0f, total_results / 5.0f);
    summary->timestamp = time(NULL);
    summary->cache_key = make_cache_key(query, "benchmark_update");
    
    cache_set(summary);
    
    hermes_log(LOG_INFO, "moa", "Research complete: %d findings, %d model scores", total_results, num_model_scores);
    return summary;
}

void moa_research_free(research_summary_t *summary) {
    if (!summary) return;
    free(summary->query);
    free(summary->intent);
    free(summary->cache_key);
    for (int i = 0; i < summary->num_findings; i++) {
        free(summary->findings[i].title);
        free(summary->findings[i].url);
        free(summary->findings[i].snippet);
        free(summary->findings[i].source);
    }
    free(summary->findings);
    for (int i = 0; i < summary->num_model_scores; i++) {
        free(summary->model_ids[i]);
    }
    free(summary->model_ids);
    free(summary->model_scores);
    free(summary);
}

void moa_apply_research_to_refs(moa_ref_model_t **refs, int *count, research_summary_t *research) {
    if (!research || research->num_model_scores == 0 || *count == 0) return;
    
    // Build current model IDs
    char **current_ids = malloc(*count * sizeof(char*));
    for (int i = 0; i < *count; i++) {
        current_ids[i] = malloc(strlen((*refs)[i].provider) + strlen((*refs)[i].model) + 2);
        sprintf(current_ids[i], "%s:%s", (*refs)[i].provider, (*refs)[i].model);
    }
    
    // Score each model: (1-w)*original_position + w*research_score
    const float w = 0.3f;  // Research weight
    typedef struct { int idx; float score; } scored_t;
    scored_t *scored = malloc(*count * sizeof(scored_t));
    
    for (int i = 0; i < *count; i++) {
        scored[i].idx = i;
        float orig_score = 1.0f - ((float)i / *count);
        float research_score = 0.5f;
        
        for (int r = 0; r < research->num_model_scores; r++) {
            if (strcmp(current_ids[i], research->model_ids[r]) == 0) {
                research_score = research->model_scores[r];
                break;
            }
        }
        
        scored[i].score = (1.0f - w) * orig_score + w * research_score;
    }
    
    // Sort by score descending
    for (int i = 0; i < *count - 1; i++) {
        for (int j = 0; j < *count - i - 1; j++) {
            if (scored[j].score < scored[j+1].score) {
                scored_t tmp = scored[j];
                scored[j] = scored[j+1];
                scored[j+1] = tmp;
            }
        }
    }
    
    // Rebuild refs in new order
    moa_ref_model_t *new_refs = malloc(*count * sizeof(moa_ref_model_t));
    for (int i = 0; i < *count; i++) {
        new_refs[i] = (*refs)[scored[i].idx];
    }
    
    // Log top 5
    char logbuf[512] = "";
    for (int i = 0; i < 5 && i < *count; i++) {
        char *colon = strchr(current_ids[scored[i].idx], ':');
        if (colon) {
            strcat(logbuf, colon + 1);
            strcat(logbuf, " ");
        }
    }
    hermes_log(LOG_INFO, "moa", "Totem pole reordered by research: %s...", logbuf);
    
    // Cleanup old refs and IDs
    free(*refs);
    for (int i = 0; i < *count; i++) {
        free(current_ids[i]);
    }
    free(current_ids);
    free(scored);
    
    *refs = new_refs;
}