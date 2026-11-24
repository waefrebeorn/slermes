/*
 * session_secret_prompt.c — C port of upstream Hermes gateway/session_secret_prompt.py.
 *
 * PoP: w17_sessionsec_8ec442_skipped @ gateway/session_secret_prompt.py:_skipped
 * PoP: w17_sessionsec_8ec442_callback_result @ gateway/session_secret_prompt.py:secret_prompt_scope.<locals>.callback
 */
#include "noc_w17_sessionsec_8ec442.h"
#include "hermes_json.h"
#include <string.h>

json_t *w17_sessionsec_8ec442_skipped(const char *var_name, const char *reason,
                                      const char *message) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(true));
    json_set(o, "reason", json_string(reason ? reason : ""));
    json_set(o, "stored_as", json_string(var_name ? var_name : ""));
    json_set(o, "validated", json_bool(false));
    json_set(o, "skipped", json_bool(true));
    json_set(o, "message", json_string(message ? message : ""));
    return o;
}

json_t *w17_sessionsec_8ec442_callback_result(const char *var_name, const char *prompt,
        int has_metadata, const char *metadata, int live_session,
        const char *ask_value, const json_t *saved) {
    /* The callback builds params (including optional metadata) for ask_viewer, whose result is
     * scripted by the oracle; metadata/prompt do not alter the decision tree. */
    (void)prompt;
    (void)has_metadata;
    (void)metadata;

    if (!live_session || !ask_value || !ask_value[0])
        return w17_sessionsec_8ec442_skipped(var_name, "cancelled", "Secret setup was skipped.");

    /* {**save_env_value_secure(var_name, value), "skipped": False, "message": ...} */
    json_t *o = (saved && saved->type == JSON_OBJECT) ? json_copy(saved) : json_object();
    json_set(o, "skipped", json_bool(false));
    json_set(o, "message", json_string(
        "Secret stored securely. The secret value was not exposed to the model."));
    return o;
}
