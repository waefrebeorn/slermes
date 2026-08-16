/*
 * prompt_caching.c — Anthropic prompt caching strategy (system_and_3).
 *
 * Port of Python agent/prompt_caching.py (79 lines).
 * Pure functions — no state, no dependencies beyond stdlib.
 *
 * Strategy: up to 4 cache_control breakpoints —
 *   system prompt (if present) + last 3 non-system messages
 *
 * MIT License — WuBu Slermes Project
 */

#include "prompt_caching.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "hermes_url_safety.h"

/* ── Cache statistics (thread-safe via simple atomics on common platforms) ── */
static int g_cache_hits = 0;
static int g_cache_misses = 0;

/* ── Cache invalidation tracking (P03) ── */
static unsigned long g_sys_prompt_hash = 0;
static int g_invalidations = 0;
static int g_prompt_set_count = 0;

/* ── Per-provider cache config (P06) ──────────────────────────────── */
#define MAX_PROVIDER_CACHE_CFG 16

typedef struct {
    char    provider[32];
    char    cache_type[32];     /* "ephemeral" or "persistent" */
    int     ttl_seconds;        /* 0 = use default */
    bool    enabled;
} provider_cache_config_t;

static provider_cache_config_t g_provider_cache[MAX_PROVIDER_CACHE_CFG];
static int g_provider_cache_count = 0;

/* Find per-provider cache config, or NULL if none */
static const provider_cache_config_t *provider_cache_find(const char *provider) {
    if (!provider) return NULL;
    for (int i = 0; i < g_provider_cache_count; i++) {
        if (strcmp(g_provider_cache[i].provider, provider) == 0)
            return &g_provider_cache[i];
    }
    return NULL;
}

/* Add or update per-provider cache config */
void prompt_cache_set_provider_config(const char *provider,
                                       const char *cache_type,
                                       int ttl_seconds, bool enabled) {
    if (!provider || !*provider) return;

    for (int i = 0; i < g_provider_cache_count; i++) {
        if (strcmp(g_provider_cache[i].provider, provider) == 0) {
            if (cache_type) snprintf(g_provider_cache[i].cache_type,
                                     sizeof(g_provider_cache[i].cache_type), "%s", cache_type);
            if (ttl_seconds >= 0) g_provider_cache[i].ttl_seconds = ttl_seconds;
            g_provider_cache[i].enabled = enabled;
            return;
        }
    }

    if (g_provider_cache_count >= MAX_PROVIDER_CACHE_CFG) return;
    snprintf(g_provider_cache[g_provider_cache_count].provider,
             sizeof(g_provider_cache[g_provider_cache_count].provider), "%s", provider);
    if (cache_type) snprintf(g_provider_cache[g_provider_cache_count].cache_type,
                             sizeof(g_provider_cache[g_provider_cache_count].cache_type), "%s", cache_type);
    g_provider_cache[g_provider_cache_count].ttl_seconds = (ttl_seconds >= 0) ? ttl_seconds : 300;
    g_provider_cache[g_provider_cache_count].enabled = enabled;
    g_provider_cache_count++;
}

/* Simple djb2 hash for fingerprinting */
static unsigned long hash_str(const char *s) {
    unsigned long h = 5381;
    if (!s) return h;
    for (const char *p = s; *p; p++)
        h = ((h << 5) + h) + (unsigned char)*p;
    return h;
}

void cache_track_hit(void) {
    __sync_fetch_and_add(&g_cache_hits, 1);
}

void cache_track_miss(void) {
    __sync_fetch_and_add(&g_cache_misses, 1);
}

void cache_reset_stats(void) {
    __sync_lock_test_and_set(&g_cache_hits, 0);
    __sync_lock_test_and_set(&g_cache_misses, 0);
}

int cache_get_hits(void) {
    return __sync_fetch_and_add(&g_cache_hits, 0);
}

int cache_get_misses(void) {
    return __sync_fetch_and_add(&g_cache_misses, 0);
}

int cache_get_total(void) {
    return cache_get_hits() + cache_get_misses();
}

char *cache_get_stats_json(void) {
    int hits = cache_get_hits();
    int misses = cache_get_misses();
    int total = hits + misses;
    double hit_rate = total > 0 ? (double)hits / total : 0.0;

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"hits\":%d,\"misses\":%d,\"total\":%d,\"hit_rate\":%.3f}",
        hits, misses, total, hit_rate);
    return strdup(buf);
}

/* ── Cache invalidation ──────────────────────────────────────── */

void cache_set_system_prompt(const char *system_content) {
    unsigned long new_hash = hash_str(system_content);

    if (g_prompt_set_count > 0 && new_hash != g_sys_prompt_hash) {
        __sync_fetch_and_add(&g_invalidations, 1);
    }

    g_sys_prompt_hash = new_hash;
    __sync_fetch_and_add(&g_prompt_set_count, 1);
}

bool cache_is_valid(void) {
    return g_invalidations == 0;
}

int cache_get_invalidations(void) {
    return g_invalidations;
}

static int g_marked_count = 0;

void cache_reset_invalidation(void) {
    g_sys_prompt_hash = 0;
    g_invalidations = 0;
    g_prompt_set_count = 0;
    g_marked_count = 0;
}

/* ── Multi-turn optimization (P04 — already implemented) ── */

void cache_set_marked_count(int count) {
    g_marked_count = count;
}

int cache_get_marked_count(void) {
    return g_marked_count;
}

/* Apply cache_control to a single message */
/* Port of Python agent/prompt_caching.py:_apply_cache_marker(). */
static void apply_cache_marker(pc_message_t *msg,
                               const char *cache_type,
                               const char *cache_ttl,
                               bool native_anthropic) {
    if (!msg) return;

    /* For tool messages, only add cache_control in native_anthropic mode */
    if (msg->role == 3 && !native_anthropic)
        return;

    msg->has_cache = true;
    snprintf(msg->cache_type, sizeof(msg->cache_type), "%s", cache_type);
    if (cache_ttl && cache_ttl[0])
        snprintf(msg->cache_ttl, sizeof(msg->cache_ttl), "%s", cache_ttl);
}

/* Port of Python agent/prompt_caching.py:_build_marker(). */
static void build_marker(const char *cache_ttl,
                         char *cache_type_out, size_t type_sz,
                         char *cache_ttl_out, size_t ttl_sz) {
    snprintf(cache_type_out, type_sz, "ephemeral");
    cache_ttl_out[0] = '\0';
    if (cache_ttl && strcmp(cache_ttl, "1h") == 0) {
        snprintf(cache_ttl_out, ttl_sz, "1h");
    }
}

/* Port of Python agent/prompt_caching.py:apply_anthropic_cache_control(). */
void apply_anthropic_cache_control(pc_message_t *messages, int *count,
                                   const char *cache_ttl,
                                   bool native_anthropic) {
    if (!messages || !count || *count <= 0) return;

    /* P04: Multi-turn optimization — skip messages already cached in prior turns */
    int start_idx = g_marked_count;
    if (start_idx >= *count) return; /* all messages already cached */

    /* Default TTL -- check per-provider config first (P06) */
    const char *ttl = cache_ttl;
    if (!ttl || !ttl[0]) {
        const provider_cache_config_t *pcfg = provider_cache_find("anthropic");
        if (pcfg && pcfg->enabled && pcfg->ttl_seconds > 0) {
            static char s_ttl_override[16];
            if (pcfg->ttl_seconds >= 3600)
                snprintf(s_ttl_override, sizeof(s_ttl_override), "%dh", pcfg->ttl_seconds / 3600);
            else if (pcfg->ttl_seconds >= 60)
                snprintf(s_ttl_override, sizeof(s_ttl_override), "%dm", pcfg->ttl_seconds / 60);
            else
                snprintf(s_ttl_override, sizeof(s_ttl_override), "%ds", pcfg->ttl_seconds);
            ttl = s_ttl_override;
        } else {
            ttl = "5m";
        }
    }

    char cache_type[32], cache_ttl_val[32];
    build_marker(ttl, cache_type, sizeof(cache_type),
                 cache_ttl_val, sizeof(cache_ttl_val));

    int breakpoints_used = 0;

    /* First message (system): only mark if new since last turn */
    if (start_idx == 0 && *count > 0 && messages[0].role == 0) {
        apply_cache_marker(&messages[0], cache_type,
                           cache_ttl_val[0] ? cache_ttl_val : NULL,
                           native_anthropic);
        breakpoints_used++;
    }

    /* Last 3 non-system messages get cache markers */
    int remaining = 4 - breakpoints_used;
    if (remaining <= 0) return;

    /* Walk backwards, collect non-system indices (only beyond start_idx) */
    int non_sys_indices[PC_MAX_MESSAGES];
    int ns_count = 0;
    for (int i = *count - 1; i >= start_idx && ns_count < remaining; i--) {
        if (messages[i].role != 0) { /* not system */
            non_sys_indices[ns_count++] = i;
        }
    }

    /* Apply markers (they're already in reverse order from the walk) */
    for (int i = 0; i < ns_count; i++) {
        apply_cache_marker(&messages[non_sys_indices[i]], cache_type,
                           cache_ttl_val[0] ? cache_ttl_val : NULL,
                           native_anthropic);
    }

    /* P04: Update marked count for next turn */
    g_marked_count = *count;
}

/* ── Cache warmup on session load (P05) ────────────────────────────────── */

/**
 * Warm up the cache when loading messages from session state.
 * Applies cache markers to restored messages so the first turn of a
 * resumed session gets a cache hit instead of a cold start.
 *
 * @param messages  Messages loaded from session (modified in-place)
 * @param count     Number of messages
 * @param cache_ttl Cache TTL string ("5m", "1h"), or NULL for default
 * @param native_anthropic Tool messages get cache_control too
 */
void cache_warmup(pc_message_t *messages, int count,
                  const char *cache_ttl, bool native_anthropic) {
    if (!messages || count <= 0) return;

    /* Apply the standard cache marking strategy */
    apply_anthropic_cache_control(messages, &count, cache_ttl, native_anthropic);

    /* The function above already set g_marked_count = count.
     * For a warmup, g_marked_count means "already cached" — next
     * call to apply_anthropic_cache_control will skip these. */
}

/* ── Anthropic prompt cache policy decision ────────────────────────────────
 *
 * Port of Python agent/agent_runtime_helpers.py:anthropic_prompt_cache_policy().
 *
 * Decide whether to apply Anthropic prompt caching and which layout to use.
 * Takes the effective provider, base_url, api_mode, and model as strings
 * (resolved from agent state by the caller).
 *
 * @param provider        Provider name (e.g. "anthropic", "openrouter")
 * @param base_url        Base URL (e.g. "https://api.anthropic.com/v1")
 * @param api_mode        API mode (e.g. "anthropic_messages", "chat_completions")
 * @param model           Model name (e.g. "claude-sonnet-4-20250514")
 * @param use_native_layout [out] Set to true when native Anthropic layout is
 *                         required (markers on inner content blocks)
 * @return true if caching should be applied for this request
 */
bool anthropic_prompt_cache_policy(const char *provider,
                                    const char *base_url,
                                    const char *api_mode,
                                    const char *model,
                                    bool *use_native_layout) {
    if (use_native_layout) *use_native_layout = false;

    /* Resolve effective values */
    const char *eff_provider = provider ? provider : "";
    const char *eff_base_url = base_url ? base_url : "";
    const char *eff_api_mode = api_mode ? api_mode : "";
    const char *eff_model = model ? model : "";

    if (!eff_model[0]) return false;

    char model_lower[256];
    char provider_lower[128];
    snprintf(model_lower, sizeof(model_lower), "%s", eff_model);
    for (int i = 0; model_lower[i]; i++)
        model_lower[i] = (char)tolower((unsigned char)model_lower[i]);
    snprintf(provider_lower, sizeof(provider_lower), "%s", eff_provider);
    for (int i = 0; provider_lower[i]; i++)
        provider_lower[i] = (char)tolower((unsigned char)provider_lower[i]);

    bool is_claude = (strstr(model_lower, "claude") != NULL);
    /* AG26: Port of Python agent/anthropic_adapter.py:_is_claude_model(). */

    bool is_openrouter = eff_base_url[0] &&
        url_host_matches(eff_base_url, "openrouter.ai");

    bool is_nous_portal = eff_base_url[0] &&
        (strstr(eff_base_url, "nousresearch") != NULL);

    bool is_anthropic_wire = (strcmp(eff_api_mode, "anthropic_messages") == 0);

    bool is_native_anthropic = false;
    if (is_anthropic_wire) {
        if (strcmp(eff_provider, "anthropic") == 0) {
            is_native_anthropic = true;
        } else if (eff_base_url[0]) {
            char *host = url_extract_hostname(eff_base_url);
            if (host) {
                if (strcmp(host, "api.anthropic.com") == 0)
                    is_native_anthropic = true;
                free(host);
            }
        }
    }

    /* Decision tree matching Python's logic */
    if (is_native_anthropic) {
        if (use_native_layout) *use_native_layout = true;
        return true;
    }

    if ((is_openrouter || is_nous_portal) && is_claude) {
        if (use_native_layout) *use_native_layout = false;
        return true;
    }

    if (is_nous_portal && strstr(model_lower, "qwen") != NULL) {
        if (use_native_layout) *use_native_layout = false;
        return true;
    }

    if (is_anthropic_wire && is_claude) {
        if (use_native_layout) *use_native_layout = true;
        return true;
    }

    /* Anthropic-wire MiniMax check */
    if (is_anthropic_wire) {
        bool is_minimax_provider = (strcmp(provider_lower, "minimax") == 0 ||
                                     strcmp(provider_lower, "minimax-cn") == 0);
        bool is_minimax_host = false;
        if (!is_minimax_provider && eff_base_url[0]) {
            char *host = url_extract_hostname(eff_base_url);
            if (host) {
                is_minimax_host = (url_host_matches(eff_base_url, "api.minimax.io") ||
                                   url_host_matches(eff_base_url, "api.minimaxi.com"));
                free(host);
            }
        }
        if (is_minimax_provider || is_minimax_host) {
            if (use_native_layout) *use_native_layout = true;
            return true;
        }
    }

    /* Alibaba-family + Qwen check */
    bool model_is_qwen = (strstr(model_lower, "qwen") != NULL);
    bool provider_is_alibaba_family = (
        strcmp(provider_lower, "opencode") == 0 ||
        strcmp(provider_lower, "opencode-zen") == 0 ||
        strcmp(provider_lower, "opencode-go") == 0 ||
        strcmp(provider_lower, "alibaba") == 0
    );
    if (provider_is_alibaba_family && model_is_qwen) {
        if (use_native_layout) *use_native_layout = false;
        return true;
    }

    return false;
}
