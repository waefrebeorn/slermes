/*
 * port_doctor.c — Faithful C11 port of pure helpers from
 * hermes_cli/doctor.py
 *
 * Ported: _termux_browser_setup_steps, _termux_install_all_fallback_notes,
 * _has_provider_env_config, _doctor_tool_availability_detail.
 * IO/env-coupled functions (_is_kanban_worker_env_gate,
 * _honcho_is_configured_for_doctor, _apply_doctor_tool_availability_overrides,
 * _has_healthy_oauth_fallback_for_apikey_provider, _missing_api_key_toolsets_for_summary,
 * run_doctor, etc.) left as REAL_GAP.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "json.h"
#include "doctor.h"

static const char *_PROVIDER_ENV_HINTS[] = {
    "DEEPINFRA_API_KEY","OPENROUTER_API_KEY","OPENAI_API_KEY","ANTHROPIC_API_KEY",
    "ANTHROPIC_TOKEN","OPENAI_BASE_URL","NOUS_API_KEY","GLM_API_KEY","ZAI_API_KEY",
    "Z_AI_API_KEY","KIMI_API_KEY","KIMI_CN_API_KEY","GMI_API_KEY","FIREWORKS_API_KEY",
    "MINIMAX_API_KEY","MINIMAX_CN_API_KEY","KILOCODE_API_KEY","DEEPSEEK_API_KEY",
    "DASHSCOPE_API_KEY","HF_TOKEN","OPENCODE_ZEN_API_KEY","OPENCODE_GO_API_KEY",
    "XIAOMI_API_KEY","TOKENHUB_API_KEY",
};
static const int _N_HINTS = 19;

/* PoP: doctor_termux_browser_setup_steps @ hermes_cli/doctor.py:_termux_browser_setup_steps */
json_t *doctor_termux_browser_setup_steps(int node_installed) {
    json_t *out = json_array();
    int step = 1;
    if (!node_installed) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d) pkg install nodejs", step);
        json_append(out, json_string(buf));
        step++;
    }
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d) npm install -g agent-browser", step);
        json_append(out, json_string(buf));
        snprintf(buf, sizeof(buf), "%d) agent-browser install", step + 1);
        json_append(out, json_string(buf));
    }
    return out;
}

/* PoP: doctor_termux_install_all_fallback_notes @ hermes_cli/doctor.py:_termux_install_all_fallback_notes */
json_t *doctor_termux_install_all_fallback_notes(void) {
    json_t *out = json_array();
    json_append(out, json_string("Termux install profile: use .[termux-all] for broad compatibility (installer default on Termux)."));
    json_append(out, json_string("Matrix E2EE extra is excluded on Termux (python-olm currently fails to build)."));
    json_append(out, json_string("Local faster-whisper extra is excluded on Termux (ctranslate2/av build path unavailable)."));
    json_append(out, json_string("STT fallback: use Groq Whisper (set GROQ_API_KEY) or OpenAI Whisper (set VOICE_TOOLS_OPENAI_KEY)."));
    return out;
}

/* PoP: doctor_has_provider_env_config @ hermes_cli/doctor.py:_has_provider_env_config */
int doctor_has_provider_env_config(const char *content) {
    if (!content) return 0;
    for (int i = 0; i < _N_HINTS; i++) {
        if (strstr(content, _PROVIDER_ENV_HINTS[i]) != NULL) return 1;
    }
    return 0;
}

/* PoP: doctor_tool_availability_detail @ hermes_cli/doctor.py:_doctor_tool_availability_detail */
const char *doctor_tool_availability_detail(const char *toolset, const char *kanban_task_env) {
    if (!toolset) return "";
    if (strcmp(toolset, "kanban") == 0 && (!kanban_task_env || !*kanban_task_env)) {
        return "(runtime-gated; loaded only for dispatcher-spawned workers)";
    }
    return "";
}
