/* port_moa_pure_helpers.c — Pure data/lookup helpers from MoA modules
 *
 * Ports:
 *   tools/mixture_of_agents_tool.py: MoAMode, get_system_prompt, get_ref_models
 *   tools/online_research.py: reorder_totem_pole_by_research
 *   tools/moa_performance.py: (data structs)
 *
 * All functions are stateless data lookups. No I/O, no state.
 */
#define _GNU_SOURCE
#include "hermes_core_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/* MoaResearchScore — a model-to-score mapping for reorder_totem_pole */
typedef struct {
    const char *model_id;
    double score;
} MoaResearchScore;

/* MoAMode enum — tool/mixture_of_agents_tool.py */

/* ════════════════════════════════════════════════════════════════════════════
 * MoAMode enum — tool/mixture_of_agents_tool.py
 * ════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    MOA_MODE_STANDARD      = 0,
    MOA_MODE_DEVIL_ADVOCATE = 1,
    MOA_MODE_TREPIDATION   = 2,
    MOA_MODE_TOKEN_MAXX    = 3,
    MOA_MODE_MEGA_TOTEM    = 4,
} MoAMode;

/* PoP: moa_mode_from_string @ tools/mixture_of_agents_tool.py:MoAMode */
MoAMode moa_mode_from_string(const char *s)
{
    if (!s) return MOA_MODE_STANDARD;
    if (strcmp(s, "devil_advocate") == 0) return MOA_MODE_DEVIL_ADVOCATE;
    if (strcmp(s, "trepidation") == 0) return MOA_MODE_TREPIDATION;
    if (strcmp(s, "token_maxx") == 0) return MOA_MODE_TOKEN_MAXX;
    if (strcmp(s, "mega_totem") == 0) return MOA_MODE_MEGA_TOTEM;
    return MOA_MODE_STANDARD;
}

/* PoP: moa_mode_to_string @ tools/mixture_of_agents_tool.py:MoAMode */
const char *moa_mode_to_string(MoAMode mode)
{
    switch (mode) {
        case MOA_MODE_STANDARD:      return "standard";
        case MOA_MODE_DEVIL_ADVOCATE: return "devil_advocate";
        case MOA_MODE_TREPIDATION:   return "trepidation";
        case MOA_MODE_TOKEN_MAXX:    return "token_maxx";
        case MOA_MODE_MEGA_TOTEM:    return "mega_totem";
        default: return "standard";
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 * RefModelConfig — struct port of Python dataclass
 * ════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *provider;
    const char *model;
    double temperature;
    int max_tokens;
    const char *reasoning_effort; /* "none" | "low" | "medium" | "high" | "xhigh" */
    const char *role;
    int benchmark_tier; /* 1=highest */
} RefModelConfig;

#define RMC(prov, mdl, temp, tok, effort, rl, tier) \
    { (prov), (mdl), (temp), (tok), (effort), (rl), (tier) }
#define RMC_END { NULL, NULL, 0, 0, NULL, NULL, 0 }

/* ════════════════════════════════════════════════════════════════════════════
 * get_system_prompt — tool/mixture_of_agents_tool.py
 * ════════════════════════════════════════════════════════════════════════════ */

static const char *MOA_STANDARD_SYSTEM =
    "You are an expert AI assistant participating in a Mixture of Agents ensemble. "
    "Your role: Provide a thorough, accurate, and well-reasoned response.";
static const char *MOA_DEVIL_SYSTEM =
    "You are a DEVIL'S ADVOCATE critic in a Mixture of Agents ensemble. "
    "Challenge the problem aggressively. Find flaws, edge cases, wrong assumptions.";
static const char *MOA_TREPIDATION_SYSTEM =
    "You are a CAUTIOUS ANALYST in a Mixture of Agents ensemble. "
    "Quantify uncertainty. Report confidence intervals. Flag disagreement.";
static const char *MOA_TOKEN_MAXX_SYSTEM =
    "You are a MAXIMUM QUALITY contributor. Exhaustive, thorough, maximum detail.";
static const char *MOA_STANDARD_AGGREGATOR_SYSTEM =
    "You are the final aggregator in a Mixture of Agents ensemble. "
    "Synthesize responses into a single, coherent, high-quality answer.";
static const char *MOA_DEVIL_AGGREGATOR_SYSTEM =
    "You are the aggregator for a Devil's Advocate Mixture of Agents. "
    "Synthesize adversarial perspectives into a robust final answer.";
static const char *MOA_TREPIDATION_AGGREGATOR_SYSTEM =
    "You are the aggregator for a Trepidation Mixture of Agents. "
    "Produce a final answer that honestly represents the epistemic state.";
static const char *MOA_TOKEN_MAXX_AGGREGATOR_SYSTEM =
    "You are the aggregator for a Token Maxxing Mixture of Agents. "
    "Synthesize the most comprehensive, thorough answer possible.";

/* PoP: moa_get_system_prompt @ tools/mixture_of_agents_tool.py:get_system_prompt */
const char *moa_get_system_prompt(MoAMode mode, int is_aggregator)
{
    if (is_aggregator) {
        switch (mode) {
            case MOA_MODE_STANDARD:      return MOA_STANDARD_AGGREGATOR_SYSTEM;
            case MOA_MODE_DEVIL_ADVOCATE: return MOA_DEVIL_AGGREGATOR_SYSTEM;
            case MOA_MODE_TREPIDATION:   return MOA_TREPIDATION_AGGREGATOR_SYSTEM;
            case MOA_MODE_TOKEN_MAXX:    return MOA_TOKEN_MAXX_AGGREGATOR_SYSTEM;
            case MOA_MODE_MEGA_TOTEM:    return MOA_TOKEN_MAXX_AGGREGATOR_SYSTEM;
            default: return MOA_STANDARD_AGGREGATOR_SYSTEM;
        }
    } else {
        switch (mode) {
            case MOA_MODE_STANDARD:      return MOA_STANDARD_SYSTEM;
            case MOA_MODE_DEVIL_ADVOCATE: return MOA_DEVIL_SYSTEM;
            case MOA_MODE_TREPIDATION:   return MOA_TREPIDATION_SYSTEM;
            case MOA_MODE_TOKEN_MAXX:    return MOA_TOKEN_MAXX_SYSTEM;
            case MOA_MODE_MEGA_TOTEM:    return MOA_STANDARD_SYSTEM;
            default: return MOA_STANDARD_SYSTEM;
        }
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 * Standard mode reference models
 * ════════════════════════════════════════════════════════════════════════════ */

static const RefModelConfig STANDARD_MODELS[] = {
    /* TIER S: Verified-callable agentic leaders */
    RMC("nvidia_nim", "z-ai/glm-5.2", 0.6, 4096, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.6, 4096, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.6, 4096, "high", "analytical", 1),
    RMC("nvidia_nim", "minimaxai/minimax-m3", 0.6, 4096, "high", "analytical", 1),
    RMC("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.6, 4096, "high", "analytical", 1),
    RMC("nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.6, 4096, "high", "analytical", 1),
    RMC("nvidia_nim", "mistralai/mistral-nemotron", 0.5, 4096, "medium", "analytical", 2),
    /* TIER A: Strong but slow/404 */
    RMC("nvidia_nim", "deepseek-ai/deepseek-v4-pro", 0.6, 4096, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "minimaxai/minimax-m2.7", 0.6, 4096, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "qwen/qwen3-next-80b-a3b-instruct", 0.6, 4096, "high", "analytical", 1),
    RMC("nvidia_nim", "moonshotai/kimi-k2.6", 0.6, 4096, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "nvidia/llama-3.1-nemotron-ultra-253b-v1", 0.6, 4096, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-4-340b-instruct", 0.6, 4096, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "nvidia/llama-3.3-nemotron-super-49b-v1.5", 0.5, 4096, "high", "creative", 2),
    RMC("nvidia_nim", "nvidia/llama-3.1-nemotron-70b-instruct", 0.5, 4096, "high", "analytical", 2),
    /* TIER B: Fast/creative */
    RMC("nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "meta/llama-4-maverick-17b-128e-instruct", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "mistralai/mixtral-8x7b-instruct-v0.1", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "poolside/laguna-xs-2.1", 0.5, 4096, "high", "analytical", 2),
    RMC("nvidia_nim", "google/diffusiongemma-26b-a4b-it", 0.5, 4096, "medium", "creative", 3),
    /* OpenRouter free tier */
    RMC("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.6, 4096, "xhigh", "analytical", 1),
    RMC("openrouter", "nvidia/nemotron-3-super-120b-a12b:free", 0.5, 4096, "high", "creative", 2),
    RMC("openrouter", "inclusionai/ring-2.6-1t:free", 0.6, 4096, "high", "creative", 2),
    /* Nous Portal fallback */
    RMC("nous_portal", "deepseek/deepseek-v4-flash", 0.5, 4096, "high", "analytical", 3),
    RMC("nous_portal", "google/gemini-3.5-flash", 0.5, 4096, "high", "analytical", 3),
    RMC("nous_portal", "meta-llama/llama-3.1-405b-instruct:free", 0.5, 4096, "medium", "synthesizer", 3),
    RMC_END
};

/* ════════════════════════════════════════════════════════════════════════════
 * Devil's Advocate mode reference models
 * ════════════════════════════════════════════════════════════════════════════ */

static const RefModelConfig DEVIL_ADVOCATE_MODELS[] = {
    RMC("nvidia_nim", "z-ai/glm-5.2", 0.8, 4096, "xhigh", "devil_advocate_1", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.8, 4096, "xhigh", "devil_advocate_2", 1),
    RMC("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.8, 4096, "high", "devil_advocate_3", 1),
    RMC("nvidia_nim", "minimaxai/minimax-m3", 0.8, 4096, "high", "devil_advocate_4", 1),
    RMC("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.8, 4096, "xhigh", "devil_advocate_5", 1),
    RMC_END
};

/* ════════════════════════════════════════════════════════════════════════════
 * Trepidation mode reference models
 * ════════════════════════════════════════════════════════════════════════════ */

static const RefModelConfig TREPIDATION_MODELS[] = {
    RMC("nvidia_nim", "z-ai/glm-5.2", 0.4, 4096, "xhigh", "critical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.4, 4096, "xhigh", "critical", 1),
    RMC("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.4, 4096, "high", "critical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.4, 4096, "high", "critical", 2),
    RMC("nvidia_nim", "minimaxai/minimax-m3", 0.4, 4096, "high", "critical", 1),
    RMC("nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.4, 4096, "high", "critical", 1),
    RMC("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.4, 4096, "xhigh", "critical", 1),
    RMC_END
};

/* ════════════════════════════════════════════════════════════════════════════
 * Token Maxx mode reference models
 * ════════════════════════════════════════════════════════════════════════════ */

static const RefModelConfig TOKEN_MAXX_MODELS[] = {
    RMC("nvidia_nim", "z-ai/glm-5.2", 0.7, 8192, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.7, 8192, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.7, 8192, "high", "analytical", 1),
    RMC("nvidia_nim", "minimaxai/minimax-m3", 0.7, 8192, "high", "analytical", 1),
    RMC("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.7, 8192, "high", "analytical", 1),
    RMC("nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.7, 8192, "high", "analytical", 1),
    RMC("nvidia_nim", "mistralai/mistral-nemotron", 0.7, 8192, "medium", "creative", 2),
    RMC("nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.7, 8192, "medium", "creative", 2),
    RMC("nvidia_nim", "meta/llama-4-maverick-17b-128e-instruct", 0.7, 8192, "medium", "creative", 2),
    RMC("nvidia_nim", "poolside/laguna-xs-2.1", 0.7, 8192, "high", "analytical", 2),
    RMC("nvidia_nim", "google/diffusiongemma-26b-a4b-it", 0.7, 8192, "medium", "creative", 2),
    RMC("nvidia_nim", "deepseek-ai/deepseek-v4-pro", 0.7, 8192, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "minimaxai/minimax-m2.7", 0.7, 8192, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "moonshotai/kimi-k2.6", 0.7, 8192, "xhigh", "analytical", 1),
    RMC("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.8, 8192, "xhigh", "analytical", 1),
    RMC("openrouter", "nvidia/nemotron-3-super-120b-a12b:free", 0.7, 8192, "xhigh", "analytical", 1),
    RMC("openrouter", "inclusionai/ring-2.6-1t:free", 0.7, 8192, "high", "creative", 2),
    RMC("nous_portal", "meta-llama/llama-3.1-405b-instruct:free", 0.7, 8192, "medium", "synthesizer", 3),
    RMC("nous_portal", "deepseek/deepseek-v4-flash", 0.7, 8192, "high", "analytical", 3),
    RMC("nous_portal", "google/gemini-3.5-flash", 0.7, 8192, "high", "analytical", 3),
    RMC_END
};

/* ════════════════════════════════════════════════════════════════════════════
 * Mega Totem mode reference models
 * ════════════════════════════════════════════════════════════════════════════ */

static const RefModelConfig MEGA_TOTEM_MODELS[] = {
    /* NVIDIA NIM TIER S (7 verified callable) */
    RMC("nvidia_nim", "z-ai/glm-5.2", 0.6, 4096, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.6, 4096, "xhigh", "analytical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.6, 4096, "high", "analytical", 1),
    RMC("nvidia_nim", "minimaxai/minimax-m3", 0.6, 4096, "high", "analytical", 1),
    RMC("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.6, 4096, "high", "analytical", 1),
    RMC("nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.6, 4096, "high", "analytical", 1),
    RMC("nvidia_nim", "mistralai/mistral-nemotron", 0.5, 4096, "medium", "creative", 2),
    /* NVIDIA NIM TIER B (7 fast/creative) */
    RMC("nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.5, 4096, "medium", "creative", 2),
    RMC("nvidia_nim", "nvidia/nemotron-3-nano-30b-a3b", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "meta/llama-4-maverick-17b-128e-instruct", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "mistralai/mixtral-8x7b-instruct-v0.1", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "poolside/laguna-xs-2.1", 0.5, 4096, "high", "analytical", 2),
    RMC("nvidia_nim", "google/diffusiongemma-26b-a4b-it", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "google/gemma-3n-e4b-it", 0.5, 4096, "medium", "creative", 3),
    /* NVIDIA NIM EXTRA */
    RMC("nvidia_nim", "google/gemma-3n-e2b-it", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "google/gemma-2-2b-it", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "nvidia/nemotron-mini-4b-instruct", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "sarvamai/sarvam-m", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "upstage/solar-10.7b-instruct", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "meta/llama-3.2-3b-instruct", 0.5, 4096, "medium", "creative", 3),
    RMC("nvidia_nim", "meta/llama-3.1-8b-instruct", 0.5, 4096, "medium", "creative", 3),
    /* OpenRouter */
    RMC("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.6, 4096, "xhigh", "analytical", 1),
    RMC("openrouter", "nvidia/nemotron-3-super-120b-a12b:free", 0.5, 4096, "high", "creative", 2),
    RMC("openrouter", "inclusionai/ring-2.6-1t:free", 0.6, 4096, "high", "creative", 2),
    /* Nous Portal */
    RMC("nous_portal", "deepseek/deepseek-v4-flash", 0.5, 4096, "high", "analytical", 3),
    RMC("nous_portal", "google/gemini-3.5-flash", 0.5, 4096, "high", "analytical", 3),
    RMC("nous_portal", "meta-llama/llama-3.1-405b-instruct:free", 0.5, 4096, "medium", "synthesizer", 3),
    RMC_END
};

/* ════════════════════════════════════════════════════════════════════════════
 * MATH mode reference models
 * ════════════════════════════════════════════════════════════════════════════ */

static const RefModelConfig MATH_MODELS[] = {
    RMC("nvidia_nim", "z-ai/glm-5.2", 0.3, 8192, "xhigh", "math_analytical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.3, 8192, "xhigh", "math_analytical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.3, 8192, "high", "math_analytical", 1),
    RMC("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.3, 8192, "high", "math_analytical", 1),
    RMC("nvidia_nim", "minimaxai/minimax-m3", 0.3, 8192, "high", "math_analytical", 1),
    RMC("nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.3, 8192, "high", "math_analytical", 1),
    RMC("nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.3, 8192, "medium", "math_creative", 2),
    RMC("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.3, 8192, "xhigh", "math_analytical", 1),
    RMC_END
};

/* ════════════════════════════════════════════════════════════════════════════
 * get_ref_models — tool/mixture_of_agents_tool.py
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: moa_get_ref_models @ tools/mixture_of_agents_tool.py:get_ref_models */
const RefModelConfig *moa_get_ref_models(MoAMode mode)
{
    switch (mode) {
        case MOA_MODE_STANDARD:      return STANDARD_MODELS;
        case MOA_MODE_DEVIL_ADVOCATE: return DEVIL_ADVOCATE_MODELS;
        case MOA_MODE_TREPIDATION:   return TREPIDATION_MODELS;
        case MOA_MODE_TOKEN_MAXX:    return TOKEN_MAXX_MODELS;
        case MOA_MODE_MEGA_TOTEM:    return MEGA_TOTEM_MODELS;
        default:                     return MATH_MODELS;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 * reorder_totem_pole_by_research — tools/online_research.py
 * ════════════════════════════════════════════════════════════════════════════ */

/* Helper: find a model in a list by name */
static int find_model_index(const char *model, const char **list, int count)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(model, list[i]) == 0) return i;
    }
    return -1;
}

/* PoP: moa_reorder_totem_pole @ tools/online_research.py:reorder_totem_pole_by_research */
int moa_reorder_totem_pole(const char * const *current_order, int count,
                            const MoaResearchScore *research_scores, int score_count,
                            double weight,
                            const char **out, int out_cap)
{
    if (!current_order || !out || count <= 0) return 0;
    if (count > out_cap) count = out_cap;

    /* Create scored pairs */
    typedef struct { const char *name; double score; } ScoredModel;
    ScoredModel scored[64];
    int n = count < 64 ? count : 64;

    for (int i = 0; i < n; i++) {
        double orig = 1.0 - (double)i / (double)n;
        double research = 0.5; /* default */
        for (int j = 0; research_scores && j < score_count; j++) {
            if (strcmp(current_order[i], research_scores[j].model_id) == 0) {
                research = research_scores[j].score;
                break;
            }
        }
        scored[i].name = current_order[i];
        scored[i].score = (1.0 - weight) * orig + weight * research;
    }

    /* Simple bubble sort descending by score */
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (scored[j].score < scored[j+1].score) {
                ScoredModel t = scored[j];
                scored[j] = scored[j+1];
                scored[j+1] = t;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        out[i] = scored[i].name;
    }
    return n;
}

/* ════════════════════════════════════════════════════════════════════════════
 * _openrouter_model_supports_tools — hermes_cli/models.py
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: openrouter_model_supports_tools @ hermes_cli/models.py:_openrouter_model_supports_tools */
bool openrouter_model_supports_tools(const char *supported_params, int has_list)
{
    if (!has_list) return true; /* permissive when field absent */
    if (!supported_params) return true;
    /* Check if "tools" appears in the comma/space-separated parameter list */
    return strstr(supported_params, "tools") != NULL;
}

/* ════════════════════════════════════════════════════════════════════════════
 * pick_silent_default_model — hermes_cli/models.py
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: pick_silent_default_model @ hermes_cli/models.py:pick_silent_default_model */
const char *pick_silent_default_model(const char * const *model_ids, int count,
                                       const char *preferred)
{
    if (!model_ids || count <= 0) return "";
    if (preferred) {
        for (int i = 0; i < count; i++) {
            if (model_ids[i] && strcmp(model_ids[i], preferred) == 0)
                return preferred;
        }
    }
    return model_ids[0] ? model_ids[0] : "";
}

/* ════════════════════════════════════════════════════════════════════════════
 * silent_default_for_provider — get_preferred_silent_default_model data
 * ════════════════════════════════════════════════════════════════════════════ */

/* PoP: silent_default_for_provider @ hermes_cli/models.py:get_preferred_silent_default_model */
const char *silent_default_for_provider(const char *provider)
{
    if (!provider) return "deepseek/deepseek-v4-flash";
    /* Map provider names to their default model */
    if (strcmp(provider, "openai") == 0) return "openai/gpt-4o";
    if (strcmp(provider, "anthropic") == 0) return "anthropic/claude-sonnet-4";
    if (strcmp(provider, "google") == 0) return "google/gemini-3.5-flash";
    if (strcmp(provider, "deepseek") == 0) return "deepseek/deepseek-chat";
    if (strcmp(provider, "openrouter") == 0) return "deepseek/deepseek-v4-flash";
    if (strcmp(provider, "nvidia_nim") == 0) return "deepseek-ai/deepseek-v4-flash";
    if (strcmp(provider, "nous_portal") == 0) return "deepseek/deepseek-v4-flash";
    if (strcmp(provider, "xai") == 0) return "xai/grok-3";
    if (strcmp(provider, "github") == 0) return "github/gpt-4o";
    return "deepseek/deepseek-v4-flash";
}