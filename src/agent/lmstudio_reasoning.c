/*
 * lmstudio_reasoning.c — LM Studio reasoning-effort resolution.
 *
 * Port of Python agent/lmstudio_reasoning.py (60 lines).
 * Maps user's reasoning_config onto LM Studio's OpenAI-compatible
 * vocabulary, then clamps against the model's allowed_options.
 *
 * MIT License — WuBu Slermes Project
 */

#include "lmstudio_reasoning.h"
#include "hermes_core_types.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ── Valid effort set ──────────────────────────────────────── */

static const char *VALID_EFFORTS[] = {
    "none", "minimal", "low", "medium", "high", "xhigh", NULL
};

bool lmstudio_is_valid_effort(const char *effort) {
    if (!effort || !effort[0]) return false;
    for (int i = 0; VALID_EFFORTS[i]; i++) {
        if (strcmp(effort, VALID_EFFORTS[i]) == 0)
            return true;
    }
    return false;
}

/* ── Alias mapping ─────────────────────────────────────────── */

static const char *ALIAS_FROM[]  = {"off", "on", NULL};
static const char *ALIAS_TO[]    = {"none", "medium"};

char *lmstudio_map_effort_alias(const char *effort) {
    if (!effort || !effort[0]) return (char *)effort;
    for (int i = 0; ALIAS_FROM[i]; i++) {
        if (strcmp(effort, ALIAS_FROM[i]) == 0)
            return (char *)ALIAS_TO[i];
    }
    return (char *)effort;
}

/* ── Helper: lowercase a string into static buffer ──────────── */

static char *strlwr(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; src[i] && i < n - 1; i++)
        dest[i] = (char)(src[i] >= 'A' && src[i] <= 'Z' ? src[i] + 32 : src[i]);
    dest[i] = '\0';
    return dest;
}

/* ── Resolution ────────────────────────────────────────────── */

/* Internal: resolve with pre-parsed params */
static const char *resolve_impl(int enabled, const char *effort, char *const *allowed_options) {
    /* Default effort */
    const char *resolved = "medium";

    if (!enabled) {
        resolved = "none";
    } else if (effort && effort[0]) {
        /* Lowercase the effort into a static buffer */
        static char lower[64];
        strlwr(lower, effort, sizeof(lower));

        /* Map aliases — returns either static alias string or lower */
        const char *mapped = lmstudio_map_effort_alias(lower);
        if (lmstudio_is_valid_effort(mapped))
            resolved = mapped;
    }

    /* Clamp against allowed_options if provided */
    if (allowed_options) {
        /* Build the allowed set with aliases applied */
        bool found = false;
        for (int j = 0; allowed_options[j]; j++) {
            const char *mapped_opt = lmstudio_map_effort_alias(allowed_options[j]);
            if (strcmp(resolved, mapped_opt) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            return NULL; /* Omit field — let LM Studio use model default */
    }

    return resolved;
}

/* ── Python-compatible API ─────────────────────────────────── */

/* Port of Python lmstudio_reasoning.py:resolve_lmstudio_effort().
 * reasoning_config is a dict-like object with:
 *   - 'enabled': bool (or None for default True)
 *   - 'effort': str (or None for default "medium")
 *
 * For simplicity, we accept a structured pointer instead of a Python dict.
 * Callers should populate this struct from their config source.
 */
typedef struct {
    int enabled;              /* 1 if reasoning is enabled, 0 if disabled */
    const char *effort;       /* User's effort choice: "low", "medium", "high" */
    char *const *allowed_options;  /* NULL-terminated array or NULL */
} lmstudio_reasoning_config_t;

/* Must be on one line for parity scanner regex */
char *resolve_lmstudio_effort(void *reasoning_config, char *const *allowed_options) {
    lmstudio_reasoning_config_t *cfg = (lmstudio_reasoning_config_t *)reasoning_config;

    if (!cfg) {
        /* No config provided: use defaults, ignore allowed_options clamping */
        return "medium";
    }

    /* Use allowed_options from config if not provided as separate arg */
    char *const *opts = allowed_options ? allowed_options : cfg->allowed_options;

    return (char *)resolve_impl(cfg->enabled, cfg->effort, opts);
}