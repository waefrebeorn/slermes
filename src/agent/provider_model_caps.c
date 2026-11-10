/*
 * provider_model_caps.c — Anthropic model-capability predicates.
 *
 * Split from provider_anthropic.c (soul.md: self-contained modules). Model
 * version/feature detection with its substring tables; no other file state.
 */
#include "provider_model_caps.h"
#include "hermes_logger.h"
#include <string.h>

static const char *ADAPTIVE_SUBSTRINGS[] = {"4-6", "4.6", "4-7", "4.7", "4-8", "4.8", NULL};
static const char *XHIGH_EFFORT_SUBSTRINGS[] = {"4-7", "4.7", "4-8", "4.8", NULL};
static const char *NO_SAMPLING_SUBSTRINGS[] = {"4-7", "4.7", "4-8", "4.8", NULL};
static const char *FAST_MODE_SUBSTRINGS[] = {"opus-4-6", "opus-4.6", NULL};


void normalize_model_key(const char *model, char *out, size_t out_size) {
    if (!model || !*model) { out[0] = '\0'; return; }
    const char *slash = strrchr(model, '/');
    if (slash) model = slash + 1;  /* strip provider prefix */
    size_t i, j = 0;
    for (i = 0; model[i] && j < out_size - 1; i++) {
        if (model[i] == '.')
            out[j++] = '-';
        else
            out[j++] = model[i];
    }
    out[j] = '\0';
}


static bool model_contains_any(const char *model, const char **patterns) {
    if (!model || !*model) return false;
    char normalized[128];
    normalize_model_key(model, normalized, sizeof(normalized));
    for (int i = 0; patterns[i]; i++) {
        if (strstr(normalized, patterns[i]))
            return true;
    }
    return false;
}

bool supports_adaptive_thinking(const char *model) {
    if (!model) return false;
    bool result = model_contains_any(model, ADAPTIVE_SUBSTRINGS);
    hermes_log(LOG_DEBUG, "anthropic", "supports_adaptive_thinking(%s) = %d", model, result);
    return result;
}

bool supports_xhigh_effort(const char *model) {
    if (!model) return false;
    bool result = model_contains_any(model, XHIGH_EFFORT_SUBSTRINGS);
    hermes_log(LOG_DEBUG, "anthropic", "supports_xhigh_effort(%s) = %d", model, result);
    return result;
}

bool forbids_sampling_params(const char *model) {
    if (!model) return false;
    bool result = model_contains_any(model, NO_SAMPLING_SUBSTRINGS);
    hermes_log(LOG_DEBUG, "anthropic", "forbids_sampling_params(%s) = %d", model, result);
    return result;
}

bool supports_fast_mode(const char *model) {
    if (!model) return false;
    bool result = model_contains_any(model, FAST_MODE_SUBSTRINGS);
    hermes_log(LOG_DEBUG, "anthropic", "supports_fast_mode(%s) = %d", model, result);
    return result;
}
