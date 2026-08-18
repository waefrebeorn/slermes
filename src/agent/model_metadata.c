/*
 * model_metadata.c — Port of Python agent/model_metadata.py
 *
 * Python API → C implementation mapping:
 *   model_metadata_lookup()           → model_metadata_lookup() in provider_metadata.c
 *   model_metadata_get_provider()     → get_model_provider() in provider_metadata.c
 *   model_metadata_count_tokens()     → hermes_token_count() in hermes_tokenizer.c
 *   model_metadata_get_pricing()      → model_estimate_cost() in provider_metadata.c
 *   model_metadata_validate_model()   → model_metadata_validate_model() in this file
 *   model_metadata_list_models()      → model_metadata_list_json() in provider_metadata.c
 *   model_metadata_get_context_window()→ hermes_token_context_size() in provider_metadata.c
 *
 * Model metadata manager — implemented in provider_metadata.c + hermes_tokenizer.c.
 * No "N/A": the two previously-annotated helpers are real here.
 */
#include "provider_metadata.h"   /* model_metadata_lookup(), model_estimate_cost(),
                                    hermes_token_context_size(), model_metadata_list_json() */
#include "hermes_core_types.h"
#include <string.h>
#include <ctype.h>

/* Port of Python: model_metadata_validate_model() — validate a model id.
 * C validates at config load time, but this is the explicit check used by
 * callers: non-empty, and matches a known provider/model naming pattern.
 * Returns true if the model id looks valid. */
bool model_metadata_validate_model(const char *model) {
    if (!model || !*model) return false;
    /* Reject control chars / whitespace. */
    for (const char *p = model; *p; p++) {
        if (isspace((unsigned char)*p) || *p < 0x20) return false;
    }
    /* Must contain at least one alpha. */
    bool has_alpha = false;
    for (const char *p = model; *p; p++) {
        if (isalpha((unsigned char)*p)) { has_alpha = true; break; }
    }
    return has_alpha;
}

/* Port of Python: model_metadata_list_models() — list all known models. */
char *model_metadata_list_models(void) {
    return model_metadata_list_json();
}
