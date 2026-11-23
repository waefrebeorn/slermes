/*
 * agent_init_params.c — real C port of upstream Hermes agent/agent_init_params.py constant tables.
 * PoP: cn_eb2be70575_constants @ agent/agent_init_params.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_eb2be70575.h"

json_t *cn_eb2be70575_constants(void) {
    json_t *out = json_object();
    json_set(out, "_PASSTHROUGH_PARAMS", ({ json_t *__a = json_array();
        json_append(__a, json_string("model"));
        json_append(__a, json_string("max_iterations"));
        json_append(__a, json_string("save_trajectories"));
        json_append(__a, json_string("verbose_logging"));
        json_append(__a, json_string("quiet_mode"));
        json_append(__a, json_string("tool_progress_mode"));
        json_append(__a, json_string("ephemeral_system_prompt"));
        json_append(__a, json_string("platform"));
        json_append(__a, json_string("skip_context_files"));
        json_append(__a, json_string("load_soul_identity"));
        json_append(__a, json_string("pass_session_id"));
        json_append(__a, json_string("log_prefix_chars"));
        json_append(__a, json_string("providers_allowed"));
        json_append(__a, json_string("providers_ignored"));
        json_append(__a, json_string("providers_order"));
        json_append(__a, json_string("provider_sort"));
        json_append(__a, json_string("provider_require_parameters"));
        json_append(__a, json_string("provider_data_collection"));
        json_append(__a, json_string("openrouter_min_coding_score"));
        json_append(__a, json_string("enabled_toolsets"));
        json_append(__a, json_string("disabled_toolsets"));
        json_append(__a, json_string("max_tokens"));
        json_append(__a, json_string("reasoning_config"));
        json_append(__a, json_string("service_tier"));
        json_append(__a, json_string("side_agent"));
        json_append(__a, json_string("memory_agent_context"));
      __a; }));
    json_set(out, "_GATEWAY_IDENTITY_PARAMS", ({ json_t *__a = json_array();
        json_append(__a, json_string("user_id"));
        json_append(__a, json_string("user_id_alt"));
        json_append(__a, json_string("user_name"));
        json_append(__a, json_string("chat_id"));
        json_append(__a, json_string("chat_name"));
        json_append(__a, json_string("chat_type"));
        json_append(__a, json_string("thread_id"));
        json_append(__a, json_string("gateway_session_key"));
      __a; }));
    json_set(out, "_CALLBACK_PARAMS", ({ json_t *__a = json_array();
        json_append(__a, json_string("tool_progress_callback"));
        json_append(__a, json_string("tool_start_callback"));
        json_append(__a, json_string("tool_complete_callback"));
        json_append(__a, json_string("tool_result_metadata_callback"));
        json_append(__a, json_string("thinking_callback"));
        json_append(__a, json_string("reasoning_callback"));
        json_append(__a, json_string("clarify_callback"));
        json_append(__a, json_string("read_terminal_callback"));
        json_append(__a, json_string("read_preview_callback"));
        json_append(__a, json_string("drive_preview_callback"));
        json_append(__a, json_string("read_window_below_callback"));
        json_append(__a, json_string("connection_callback"));
        json_append(__a, json_string("tour_callback"));
        json_append(__a, json_string("theme_callback"));
        json_append(__a, json_string("setup_beat_callback"));
        json_append(__a, json_string("step_callback"));
        json_append(__a, json_string("stream_delta_callback"));
        json_append(__a, json_string("interim_assistant_callback"));
        json_append(__a, json_string("status_callback"));
        json_append(__a, json_string("notice_callback"));
        json_append(__a, json_string("notice_clear_callback"));
        json_append(__a, json_string("event_callback"));
        json_append(__a, json_string("reaction_callback"));
        json_append(__a, json_string("tool_gen_callback"));
      __a; }));
    return out;
}
