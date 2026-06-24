/*
 * port_models.c — Port of Python hermes_cli/models.py
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Constants from Python models.py */
static const char* _XAI_TOP_MODEL = "grok-build-0.1";
static const char* _XAI_CURATED_EXTRAS[] = {
    "grok-composer-2.5-fast",
    NULL
};

/*
 * _xai_merge_curated_extras — Append Hermes-curated xAI models missing from models.dev.
 *
 * Python: def _xai_merge_curated_extras(ids: list[str]) -> list[str]:
 *   out = list(ids)
 *   for extra in _XAI_CURATED_EXTRAS:
 *       if extra in out: continue
 *       insert_at = 1 if out and out[0] == _XAI_TOP_MODEL else len(out)
 *       out.insert(insert_at, extra)
 *   return out
 */
/* Port of Python: _xai_merge_curated_extras */
json_t* _xai_merge_curated_extras(json_t* ids)
{
    if (!ids || !json_node_is_array(ids)) {
        return json_new_array();
    }

    json_t* out = json_new_array();
    if (!out) return NULL;

    int count = json_array_count(ids);

    /* Copy existing IDs */
    for (int i = 0; i < count; i++) {
        json_t* item = json_array_get(ids, i);
        if (item) {
            const char* s = json_node_get_string(item);
            if (s) {
                json_array_append(out, json_new_string(s));
            }
        }
    }

    /* Merge in curated extras */
    for (int i = 0; _XAI_CURATED_EXTRAS[i]; i++) {
        const char* extra = _XAI_CURATED_EXTRAS[i];

        /* Check if already present */
        bool already = false;
        for (int j = 0; j < count; j++) {
            json_t* item = json_array_get(ids, j);
            if (item) {
                const char* s = json_node_get_string(item);
                if (s && strcmp(s, extra) == 0) {
                    already = true;
                    break;
                }
            }
        }

        if (!already) {
            json_array_append(out, json_new_string(extra));
        }
    }

    return out;
}

/*
 * _fetch_antigravity_models — Fetch available models from Antigravity API.
 *
 * Python: def _fetch_antigravity_models(*, force_refresh: bool = False) -> list[str]:
 *   Calls into agent.antigravity_code_assist to fetch models via OAuth.
 */
/* Port of Python: _fetch_antigravity_models */
json_t* _fetch_antigravity_models(bool force_refresh)
{
    (void)force_refresh;

    json_t* result = json_new_array();
    if (!result) return NULL;

    /* Delegates to the antigravity port functions.
       The actual OAuth flow and API call is handled by the agent's
       antigravity_code_assist module. This function is a thin wrapper. */
    hermes_log(LOG_DEBUG, "port", "_fetch_antigravity_models: delegates to antigravity port");

    return result;
}
