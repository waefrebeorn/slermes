/*
 * lmstudio_reasoning.c — LM Studio reasoning-effort resolution.
 *
 * Port of Python agent/lmstudio_reasoning.py (60 lines).
 * Maps user's reasoning_config dict onto LM Studio's OpenAI-compatible
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

const char *lmstudio_map_effort_alias(const char *effort) {
    if (!effort || !effort[0]) return effort;
    for (int i = 0; ALIAS_FROM[i]; i++) {
        if (strcmp(effort, ALIAS_FROM[i]) == 0)
            return ALIAS_TO[i];
    }
    return effort;
}

/* ── Helpers ───────────────────────────────────────────────── */

static void strlwr(char *dest, const char *src, size_t n) {
    if (!dest || n == 0) return;
    size_t i;
    for (i = 0; src[i] && i + 1 < n; i++)
        dest[i] = (char)(src[i] >= 'A' && src[i] <= 'Z' ? src[i] + 32 : src[i]);
    dest[i] = '\0';
}

/* ── Resolution ────────────────────────────────────────────── */

/*
 * resolve_lmstudio_effort — Return the reasoning_effort string to send to
 * LM Studio, or NULL meaning "omit the field".
 *
 * Mirrors Python:
 *   def resolve_lmstudio_effort(
 *       reasoning_config: Optional[dict],
 *       allowed_options: Optional[List[str]],
 *   ) -> Optional[str]:
 */
const char *resolve_lmstudio_effort(const lmstudio_reasoning_config_t *reasoning_config, const char *const *allowed_options) {
    /* Default effort */
    const char *resolved = "medium";

    if (!reasoning_config) {
        /* No config provided: use defaults, ignore allowed_options clamping */
        return resolved;
    }

    bool enabled = reasoning_config->enabled;
    const char *effort = reasoning_config->effort;

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
