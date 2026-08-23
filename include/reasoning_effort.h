/*
 * reasoning_effort.h — C11 port of agent/reasoning_effort.py.
 *
 * Canonical reasoning-effort vocabulary and wire clamping.
 */
#ifndef SLERMES_REASONING_EFFORT_H
#define SLERMES_REASONING_EFFORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Canonical low→high ordering used for nearest-level clamping (EFFORT_LADDER). */
extern const char *const RE_EFFORT_LADDER[];
extern const size_t RE_EFFORT_LADDER_N;

/* Named wire vocabularies (each declared as data, mirroring the Python tuples). */
extern const char *const RE_OPENAI_COMPAT_WIRE_EFFORTS[];   /* OPENAI_COMPAT_WIRE_EFFORTS */
extern const size_t RE_OPENAI_COMPAT_WIRE_EFFORTS_N;
extern const char *const RE_CODEX_GPT56_EFFORTS[];          /* CODEX_GPT56_EFFORTS */
extern const size_t RE_CODEX_GPT56_EFFORTS_N;
extern const char *const RE_CODEX_LEGACY_EFFORTS[];         /* CODEX_LEGACY_EFFORTS */
extern const size_t RE_CODEX_LEGACY_EFFORTS_N;
extern const char *const RE_XAI_GROK46_EFFORTS[];           /* XAI_GROK46_EFFORTS */
extern const size_t RE_XAI_GROK46_EFFORTS_N;
extern const char *const RE_XAI_LEGACY_EFFORTS[];           /* XAI_LEGACY_EFFORTS */
extern const size_t RE_XAI_LEGACY_EFFORTS_N;
extern const char *const RE_ACTUAL_RELAY_EFFORTS[];         /* ACTUAL_RELAY_EFFORTS */
extern const size_t RE_ACTUAL_RELAY_EFFORTS_N;
extern const char *const RE_KIMI_K3_EFFORTS[];              /* KIMI_K3_EFFORTS */
extern const size_t RE_KIMI_K3_EFFORTS_N;
extern const char *const RE_KIMI_K2_EFFORTS[];              /* KIMI_K2_EFFORTS */
extern const size_t RE_KIMI_K2_EFFORTS_N;
extern const char *const RE_OX_ALPHA_EFFORTS[];             /* OX_ALPHA_EFFORTS */
extern const size_t RE_OX_ALPHA_EFFORTS_N;
extern const char *const RE_TOKENHUB_EFFORTS[];             /* TOKENHUB_EFFORTS */
extern const size_t RE_TOKENHUB_EFFORTS_N;
extern const char *const RE_GLM52_EFFORTS[];                /* GLM52_EFFORTS */
extern const size_t RE_GLM52_EFFORTS_N;
extern const char *const RE_GLM53_EFFORTS[];                /* GLM53_EFFORTS */
extern const size_t RE_GLM53_EFFORTS_N;
extern const char *const RE_DEEPSEEK_V4_EFFORTS[];          /* DEEPSEEK_V4_EFFORTS */
extern const size_t RE_DEEPSEEK_V4_EFFORTS_N;
extern const char *const RE_OLLAMA_CLOUD_EFFORTS[];         /* OLLAMA_CLOUD_EFFORTS */
extern const size_t RE_OLLAMA_CLOUD_EFFORTS_N;
extern const char *const RE_META_AI_EFFORTS[];              /* META_AI_EFFORTS */
extern const size_t RE_META_AI_EFFORTS_N;
extern const char *const RE_SOLAR_EFFORTS[];                /* SOLAR_EFFORTS */
extern const size_t RE_SOLAR_EFFORTS_N;

/* Declared translation overrides ({from, to}), consulted before nearest-weaker. */
typedef struct {
    const char *from;
    const char *to;
} re_override_t;

extern const re_override_t RE_OX_ALPHA_OVERRIDES[];         /* OX_ALPHA_OVERRIDES */
extern const size_t RE_OX_ALPHA_OVERRIDES_N;
extern const re_override_t RE_KIMI_K3_OVERRIDES[];          /* KIMI_K3_OVERRIDES */
extern const size_t RE_KIMI_K3_OVERRIDES_N;
extern const re_override_t RE_GLM52_OVERRIDES[];            /* GLM52_OVERRIDES */
extern const size_t RE_GLM52_OVERRIDES_N;
extern const re_override_t RE_GLM53_OVERRIDES[];            /* GLM53_OVERRIDES */
extern const size_t RE_GLM53_OVERRIDES_N;
extern const re_override_t RE_DEEPSEEK_V4_OVERRIDES[];      /* DEEPSEEK_V4_OVERRIDES */
extern const size_t RE_DEEPSEEK_V4_OVERRIDES_N;
extern const re_override_t RE_OLLAMA_CLOUD_OVERRIDES[];     /* OLLAMA_CLOUD_OVERRIDES */
extern const size_t RE_OLLAMA_CLOUD_OVERRIDES_N;

/* codex_supported_efforts @ agent/reasoning_effort.py:codex_supported_efforts */
const char *const *codex_supported_efforts(const char *model, size_t *out_n);

/* kimi_supported_efforts @ agent/reasoning_effort.py:kimi_supported_efforts */
const char *const *kimi_supported_efforts(const char *model, size_t *out_n);

/*
 * clamp_effort @ agent/reasoning_effort.py:clamp_effort
 *
 * Clamp a requested reasoning effort onto a wire's supported levels.
 * Returns either the ORIGINAL `effort` pointer (pass-through) or a static
 * table literal — never an allocation. NULL-safe on every argument.
 */
const char *clamp_effort(const char *effort,
                         const char *const *supported, size_t supported_n,
                         const re_override_t *overrides, size_t overrides_n);

/*
 * requested_effort @ agent/reasoning_effort.py:requested_effort
 *
 * Extract the user's explicit effort from a reasoning config (JSON object),
 * lowercased into a malloc'd string (caller frees), or NULL when absent /
 * malformed / disabled / empty.
 */
char *requested_effort(const void *reasoning_config); /* json_t* */

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_REASONING_EFFORT_H */
