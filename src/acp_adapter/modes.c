/*
 * modes.c — C port of upstream Hermes `acp_adapter/modes.py`.
 *
 * PoP: acp_modes_default_mode @ acp_adapter/modes.py:DEFAULT_MODE
 * PoP: acp_modes_default_edit_approval_policy @ acp_adapter/modes.py:DEFAULT_EDIT_APPROVAL_POLICY
 * PoP: acp_modes_edit_approval_policy_config_id @ acp_adapter/modes.py:EDIT_APPROVAL_POLICY_CONFIG_ID
 * PoP: acp_modes_table @ acp_adapter/modes.py:MODES
 * PoP: acp_modes_mode_to_policy @ acp_adapter/modes.py:MODE_TO_EDIT_APPROVAL_POLICY
 * PoP: acp_modes_policy_to_mode @ acp_adapter/modes.py:EDIT_APPROVAL_POLICY_TO_MODE
 * PoP: acp_modes_policy_for_mode @ acp_adapter/modes.py:MODE_TO_EDIT_APPROVAL_POLICY.get
 * PoP: acp_modes_mode_for_policy @ acp_adapter/modes.py:EDIT_APPROVAL_POLICY_TO_MODE.get
 *
 * The three tables are the single source of truth. Lookups mirror Python
 * dict.get semantics: an absent key returns the caller's fallback (or NULL,
 * emitted as JSON null, for the two-argument form).
 */
#include "hermes_json.h"
#include <stddef.h>
#include <string.h>

typedef struct {
    const char *mode;
    const char *policy;
    const char *display_name;
    const char *description;
} acp_mode_spec_t;

static const acp_mode_spec_t ACP_MODE_SPECS[] = {
    { "default", "ask", "Default", "Ask before edits." },
    { "accept_edits", "workspace_session", "Accept Edits",
      "Auto-allow workspace and temp-dir edits; still asks for sensitive paths." },
    { "dont_ask", "session", "Don't Ask",
      "Auto-allow file edits for this session except sensitive paths." },
};
#define ACP_MODE_COUNT (sizeof(ACP_MODE_SPECS) / sizeof(ACP_MODE_SPECS[0]))

const char *acp_modes_default_mode(void) { return "default"; }
const char *acp_modes_default_edit_approval_policy(void) { return "ask"; }
const char *acp_modes_edit_approval_policy_config_id(void) { return "edit_approval_policy"; }

const char *acp_modes_policy_for_mode(const char *mode, const char *fallback) {
    if (mode) {
        for (size_t i = 0; i < ACP_MODE_COUNT; i++) {
            if (!strcmp(mode, ACP_MODE_SPECS[i].mode)) return ACP_MODE_SPECS[i].policy;
        }
    }
    return fallback;
}

const char *acp_modes_mode_for_policy(const char *policy) {
    if (policy) {
        for (size_t i = 0; i < ACP_MODE_COUNT; i++) {
            if (!strcmp(policy, ACP_MODE_SPECS[i].policy)) return ACP_MODE_SPECS[i].mode;
        }
    }
    return NULL;
}

json_t *acp_modes_table(void) {
    json_t *out = json_object();
    for (size_t i = 0; i < ACP_MODE_COUNT; i++) {
        json_t *row = json_array();
        json_append(row, json_string(ACP_MODE_SPECS[i].policy));
        json_append(row, json_string(ACP_MODE_SPECS[i].display_name));
        json_append(row, json_string(ACP_MODE_SPECS[i].description));
        json_set(out, ACP_MODE_SPECS[i].mode, row);
    }
    return out;
}

json_t *acp_modes_mode_to_policy(void) {
    json_t *out = json_object();
    for (size_t i = 0; i < ACP_MODE_COUNT; i++)
        json_set(out, ACP_MODE_SPECS[i].mode, json_string(ACP_MODE_SPECS[i].policy));
    return out;
}

json_t *acp_modes_policy_to_mode(void) {
    json_t *out = json_object();
    for (size_t i = 0; i < ACP_MODE_COUNT; i++)
        json_set(out, ACP_MODE_SPECS[i].policy, json_string(ACP_MODE_SPECS[i].mode));
    return out;
}
