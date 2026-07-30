/*
 * config_io.c -- extracted from cli/config.c monolith.
 * Real implementation of one config-lifecycle concern; public
 * hermes_config_* protos stay in include/hermes_core_types.h.
 */

#include "hermes_core_types.h"
#include "config_schema.h"
#include "hermes_yaml.h"
#include "hermes_json.h"
#include "hermes_auth.h"
#include "provider_metadata.h"
#include "curses_widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/stat.h>

static bool is_set_str(const char *s) { return s && s[0] != '\0'; }
static bool is_set_int(int v) { return v != 0; }
static bool is_set_bool(bool v) { return v; }  /* bools always overwrite if true */

void hermes_config_merge(hermes_config_t *dst, const hermes_config_t *src) {
    /* Provider */
    if (is_set_str(src->provider_cfg.model))
        snprintf(dst->provider_cfg.model, sizeof(dst->provider_cfg.model), "%s", src->provider_cfg.model);
    if (is_set_str(src->provider_cfg.provider))
        snprintf(dst->provider_cfg.provider, sizeof(dst->provider_cfg.provider), "%s", src->provider_cfg.provider);
    if (is_set_str(src->provider_cfg.base_url))
        snprintf(dst->provider_cfg.base_url, sizeof(dst->provider_cfg.base_url), "%s", src->provider_cfg.base_url);
    if (is_set_str(src->provider_cfg.api_key))
        snprintf(dst->provider_cfg.api_key, sizeof(dst->provider_cfg.api_key), "%s", src->provider_cfg.api_key);
    if (is_set_str(src->provider_cfg.api_mode))
        snprintf(dst->provider_cfg.api_mode, sizeof(dst->provider_cfg.api_mode), "%s", src->provider_cfg.api_mode);
    if (is_set_str(src->provider_cfg.fallback_model))
        snprintf(dst->provider_cfg.fallback_model, sizeof(dst->provider_cfg.fallback_model), "%s", src->provider_cfg.fallback_model);
    if (is_set_str(src->provider_cfg.service_tier))
        snprintf(dst->provider_cfg.service_tier, sizeof(dst->provider_cfg.service_tier), "%s", src->provider_cfg.service_tier);
    if (src->provider_cfg.temperature >= 0.01f)
        dst->provider_cfg.temperature = src->provider_cfg.temperature;
    if (src->provider_cfg.top_p >= 0.01f)
        dst->provider_cfg.top_p = src->provider_cfg.top_p;
    if (is_set_int(src->provider_cfg.max_tokens))
        dst->provider_cfg.max_tokens = src->provider_cfg.max_tokens;

    /* Display */
    if (is_set_str(src->display.skin))
        snprintf(dst->display.skin, sizeof(dst->display.skin), "%s", src->display.skin);
    if (is_set_str(src->display.banner))
        snprintf(dst->display.banner, sizeof(dst->display.banner), "%s", src->display.banner);
    if (is_set_str(src->display.spinner_style))
        snprintf(dst->display.spinner_style, sizeof(dst->display.spinner_style), "%s", src->display.spinner_style);
    if (is_set_str(src->display.indicator))
        snprintf(dst->display.indicator, sizeof(dst->display.indicator), "%s", src->display.indicator);
    if (is_set_str(src->display.language))
        snprintf(dst->display.language, sizeof(dst->display.language), "%s", src->display.language);
    if (is_set_str(src->display.personality))
        snprintf(dst->display.personality, sizeof(dst->display.personality), "%s", src->display.personality);
    if (is_set_bool(src->display.stream))
        dst->display.stream = src->display.stream;
    if (is_set_bool(src->display.show_reasoning))
        dst->display.show_reasoning = src->display.show_reasoning;
    if (is_set_bool(src->display.compact))
        dst->display.compact = src->display.compact;
    if (is_set_bool(src->display.show_cost))
        dst->display.show_cost = src->display.show_cost;
    if (is_set_bool(src->display.timestamps))
        dst->display.timestamps = src->display.timestamps;

    /* Agent */
    if (is_set_int(src->agent.max_iterations))
        dst->agent.max_iterations = src->agent.max_iterations;
    if (is_set_int(src->agent.max_tool_calls_round))
        dst->agent.max_tool_calls_round = src->agent.max_tool_calls_round;
    if (is_set_int(src->agent.verbose_level))
        dst->agent.verbose_level = src->agent.verbose_level;
    if (is_set_int(src->agent.api_max_retries))
        dst->agent.api_max_retries = src->agent.api_max_retries;
    if (is_set_int(src->agent.clarify_timeout))
        dst->agent.clarify_timeout = src->agent.clarify_timeout;
    if (src->agent.compress_threshold >= 0.01f)
        dst->agent.compress_threshold = src->agent.compress_threshold;
    if (is_set_bool(src->agent.fast_mode))
        dst->agent.fast_mode = src->agent.fast_mode;
    if (is_set_bool(src->agent.quiet_mode))
        dst->agent.quiet_mode = src->agent.quiet_mode;
    if (is_set_str(src->agent.system_prompt))
        snprintf(dst->agent.system_prompt, sizeof(dst->agent.system_prompt), "%s", src->agent.system_prompt);
    if (is_set_str(src->agent.profile))
        snprintf(dst->agent.profile, sizeof(dst->agent.profile), "%s", src->agent.profile);
    if (is_set_str(src->agent.reasoning_effort))
        snprintf(dst->agent.reasoning_effort, sizeof(dst->agent.reasoning_effort), "%s", src->agent.reasoning_effort);

    /* Tools */
    if (is_set_str(src->tools.approval_mode))
        snprintf(dst->tools.approval_mode, sizeof(dst->tools.approval_mode), "%s", src->tools.approval_mode);
    if (is_set_int(src->tools.approval_timeout))
        dst->tools.approval_timeout = src->tools.approval_timeout;
    if (is_set_int(src->tools.max_result_size))
        dst->tools.max_result_size = src->tools.max_result_size;
    if (is_set_int(src->tools.terminal_timeout))
        dst->tools.terminal_timeout = src->tools.terminal_timeout;
    if (is_set_str(src->tools.vision_model)) {
        snprintf(dst->tools.vision_model, sizeof(dst->tools.vision_model), "%s", src->tools.vision_model);
        snprintf(dst->auxiliary.vision.model, sizeof(dst->auxiliary.vision.model), "%s", src->tools.vision_model);
    }
    if (is_set_str(src->tools.terminal_backend))
        snprintf(dst->tools.terminal_backend, sizeof(dst->tools.terminal_backend), "%s", src->tools.terminal_backend);
    if (is_set_int(src->tools.web_search_timeout))
        dst->tools.web_search_timeout = src->tools.web_search_timeout;
    if (is_set_str(src->tools.web_backend))
        snprintf(dst->tools.web_backend, sizeof(dst->tools.web_backend), "%s", src->tools.web_backend);
    if (is_set_str(src->tools.web_search_backend))
        snprintf(dst->tools.web_search_backend, sizeof(dst->tools.web_search_backend), "%s", src->tools.web_search_backend);
    if (is_set_str(src->tools.web_extract_backend))
        snprintf(dst->tools.web_extract_backend, sizeof(dst->tools.web_extract_backend), "%s", src->tools.web_extract_backend);

    /* Delegation */
    if (is_set_int(src->delegation.max_concurrent_children))
        dst->delegation.max_concurrent_children = src->delegation.max_concurrent_children;
    if (is_set_int(src->delegation.max_spawn_depth))
        dst->delegation.max_spawn_depth = src->delegation.max_spawn_depth;
    if (is_set_int(src->delegation.child_timeout))
        dst->delegation.child_timeout = src->delegation.child_timeout;
    if (is_set_str(src->delegation.child_model))
        snprintf(dst->delegation.child_model, sizeof(dst->delegation.child_model), "%s", src->delegation.child_model);
    if (is_set_str(src->delegation.child_provider))
        snprintf(dst->delegation.child_provider, sizeof(dst->delegation.child_provider), "%s", src->delegation.child_provider);
    if (is_set_int(src->delegation.child_max_turns))
        dst->delegation.child_max_turns = src->delegation.child_max_turns;

    /* Browser */
    if (is_set_str(src->browser_cfg.cdp_url))
        snprintf(dst->browser_cfg.cdp_url, sizeof(dst->browser_cfg.cdp_url), "%s", src->browser_cfg.cdp_url);
    if (is_set_str(src->browser_cfg.browser_type))
        snprintf(dst->browser_cfg.browser_type, sizeof(dst->browser_cfg.browser_type), "%s", src->browser_cfg.browser_type);
    if (is_set_int(src->browser_cfg.timeout))
        dst->browser_cfg.timeout = src->browser_cfg.timeout;
    if (is_set_int(src->browser_cfg.viewport_width))
        dst->browser_cfg.viewport_width = src->browser_cfg.viewport_width;
    if (is_set_int(src->browser_cfg.viewport_height))
        dst->browser_cfg.viewport_height = src->browser_cfg.viewport_height;
    dst->browser_cfg.enable_javascript = src->browser_cfg.enable_javascript;

    /* Memory */
    if (is_set_str(src->memory.provider))
        snprintf(dst->memory.provider, sizeof(dst->memory.provider), "%s", src->memory.provider);
    if (is_set_int(src->memory.char_limit))
        dst->memory.char_limit = src->memory.char_limit;
    if (is_set_int(src->memory.user_char_limit))
        dst->memory.user_char_limit = src->memory.user_char_limit;
    if (is_set_int(src->memory.ttl_days))
        dst->memory.ttl_days = src->memory.ttl_days;
    if (is_set_int(src->memory.search_limit))
        dst->memory.search_limit = src->memory.search_limit;
    dst->memory.auto_save = src->memory.auto_save;
    dst->memory.compression_enabled = src->memory.compression_enabled;

    /* Compression */
    if (is_set_str(src->compression.model))
        snprintf(dst->compression.model, sizeof(dst->compression.model), "%s", src->compression.model);
    if (is_set_str(src->compression.strategy))
        snprintf(dst->compression.strategy, sizeof(dst->compression.strategy), "%s", src->compression.strategy);
    if (src->compression.target_ratio >= 0.01f)
        dst->compression.target_ratio = src->compression.target_ratio;
    if (is_set_int(src->compression.min_messages))
        dst->compression.min_messages = src->compression.min_messages;
    dst->compression.preserve_system = src->compression.preserve_system;
    if (is_set_int(src->compression.protect_last_n))
        dst->compression.protect_last_n = src->compression.protect_last_n;
    if (is_set_int(src->compression.protect_first_n))
        dst->compression.protect_first_n = src->compression.protect_first_n;
    if (is_set_int(src->compression.hygiene_hard_message_limit))
        dst->compression.hygiene_hard_message_limit = src->compression.hygiene_hard_message_limit;
    dst->compression.abort_on_summary_failure = src->compression.abort_on_summary_failure;

    /* Cron */
    if (is_set_str(src->cron.dir))
        snprintf(dst->cron.dir, sizeof(dst->cron.dir), "%s", src->cron.dir);
    if (is_set_int(src->cron.max_concurrent_jobs))
        dst->cron.max_concurrent_jobs = src->cron.max_concurrent_jobs;
    if (is_set_int(src->cron.job_timeout))
        dst->cron.job_timeout = src->cron.job_timeout;
    if (is_set_int(src->cron.retention_days))
        dst->cron.retention_days = src->cron.retention_days;
    dst->cron.notify_on_failure = src->cron.notify_on_failure;

    /* Notification */
    if (is_set_str(src->notification.provider))
        snprintf(dst->notification.provider, sizeof(dst->notification.provider), "%s", src->notification.provider);
    if (is_set_str(src->notification.sound))
        snprintf(dst->notification.sound, sizeof(dst->notification.sound), "%s", src->notification.sound);
    dst->notification.on_complete = src->notification.on_complete;
    dst->notification.on_error = src->notification.on_error;
    dst->notification.on_approval = src->notification.on_approval;

    /* Plugin */
    if (is_set_str(src->plugin.dirs))
        snprintf(dst->plugin.dirs, sizeof(dst->plugin.dirs), "%s", src->plugin.dirs);
    if (is_set_str(src->plugin.enabled))
        snprintf(dst->plugin.enabled, sizeof(dst->plugin.enabled), "%s", src->plugin.enabled);

    /* Security */
    if (is_set_int(src->security.tirith_timeout))
        dst->security.tirith_timeout = src->security.tirith_timeout;
    dst->security.tirith_enabled = src->security.tirith_enabled;
    dst->security.allow_private_urls = src->security.allow_private_urls;
    dst->security.website_blocklist_enabled = src->security.website_blocklist_enabled;

    /* Session */
    if (is_set_int(src->session.retention_days))
        dst->session.retention_days = src->session.retention_days;
    if (is_set_int(src->session.auto_save_interval))
        dst->session.auto_save_interval = src->session.auto_save_interval;
    dst->session.compress = src->session.compress;
    dst->session.store_trajectories = src->session.store_trajectories;

    /* MCP */
    if (is_set_int(src->mcp.timeout))
        dst->mcp.timeout = src->mcp.timeout;
    if (is_set_int(src->mcp.max_tools))
        dst->mcp.max_tools = src->mcp.max_tools;
    if (is_set_str(src->mcp.credential_store))
        snprintf(dst->mcp.credential_store, sizeof(dst->mcp.credential_store), "%s", src->mcp.credential_store);
    dst->mcp.auth_enabled = src->mcp.auth_enabled;

    /* Auxiliary merge */
    #define M_AUX_STR(task, field) if (is_set_str(src->auxiliary.task.field)) snprintf(dst->auxiliary.task.field, sizeof(dst->auxiliary.task.field), "%s", src->auxiliary.task.field)
    #define M_AUX_INT(task, field) if (is_set_int(src->auxiliary.task.field)) dst->auxiliary.task.field = src->auxiliary.task.field
    M_AUX_STR(vision, provider); M_AUX_STR(vision, model); M_AUX_STR(vision, base_url);
    M_AUX_STR(vision, api_key); M_AUX_INT(vision, timeout); M_AUX_STR(vision, extra_body);
    if (is_set_int(src->auxiliary.vision_download_timeout)) dst->auxiliary.vision_download_timeout = src->auxiliary.vision_download_timeout;
    M_AUX_STR(web_extract, provider); M_AUX_STR(web_extract, model); M_AUX_STR(web_extract, base_url);
    M_AUX_STR(web_extract, api_key); M_AUX_INT(web_extract, timeout); M_AUX_STR(web_extract, extra_body);
    M_AUX_STR(compression, provider); M_AUX_STR(compression, model); M_AUX_STR(compression, base_url);
    M_AUX_STR(compression, api_key); M_AUX_INT(compression, timeout); M_AUX_STR(compression, extra_body);
    M_AUX_STR(skills_hub, provider); M_AUX_STR(skills_hub, model); M_AUX_STR(skills_hub, base_url);
    M_AUX_STR(skills_hub, api_key); M_AUX_INT(skills_hub, timeout); M_AUX_STR(skills_hub, extra_body);
    M_AUX_STR(approval, provider); M_AUX_STR(approval, model); M_AUX_STR(approval, base_url);
    M_AUX_STR(approval, api_key); M_AUX_INT(approval, timeout); M_AUX_STR(approval, extra_body);
    M_AUX_STR(mcp, provider); M_AUX_STR(mcp, model); M_AUX_STR(mcp, base_url);
    M_AUX_STR(mcp, api_key); M_AUX_INT(mcp, timeout); M_AUX_STR(mcp, extra_body);
    M_AUX_STR(title_generation, provider); M_AUX_STR(title_generation, model); M_AUX_STR(title_generation, base_url);
    M_AUX_STR(title_generation, api_key); M_AUX_INT(title_generation, timeout); M_AUX_STR(title_generation, extra_body);
    M_AUX_STR(triage_specifier, provider); M_AUX_STR(triage_specifier, model); M_AUX_STR(triage_specifier, base_url);
    M_AUX_STR(triage_specifier, api_key); M_AUX_INT(triage_specifier, timeout); M_AUX_STR(triage_specifier, extra_body);
    M_AUX_STR(kanban_decomposer, provider); M_AUX_STR(kanban_decomposer, model); M_AUX_STR(kanban_decomposer, base_url);
    M_AUX_STR(kanban_decomposer, api_key); M_AUX_INT(kanban_decomposer, timeout); M_AUX_STR(kanban_decomposer, extra_body);
    M_AUX_STR(profile_describer, provider); M_AUX_STR(profile_describer, model); M_AUX_STR(profile_describer, base_url);
    M_AUX_STR(profile_describer, api_key); M_AUX_INT(profile_describer, timeout); M_AUX_STR(profile_describer, extra_body);
    M_AUX_STR(curator, provider); M_AUX_STR(curator, model); M_AUX_STR(curator, base_url);
    M_AUX_STR(curator, api_key); M_AUX_INT(curator, timeout); M_AUX_STR(curator, extra_body);
    #undef M_AUX_STR
    #undef M_AUX_INT

    /* TTS merge */
    if (is_set_str(src->tts.provider)) snprintf(dst->tts.provider, sizeof(dst->tts.provider), "%s", src->tts.provider);
    if (is_set_str(src->tts.edge_voice)) snprintf(dst->tts.edge_voice, sizeof(dst->tts.edge_voice), "%s", src->tts.edge_voice);
    if (is_set_str(src->tts.elevenlabs_voice_id)) snprintf(dst->tts.elevenlabs_voice_id, sizeof(dst->tts.elevenlabs_voice_id), "%s", src->tts.elevenlabs_voice_id);
    if (is_set_str(src->tts.elevenlabs_model_id)) snprintf(dst->tts.elevenlabs_model_id, sizeof(dst->tts.elevenlabs_model_id), "%s", src->tts.elevenlabs_model_id);
    if (is_set_str(src->tts.openai_model)) snprintf(dst->tts.openai_model, sizeof(dst->tts.openai_model), "%s", src->tts.openai_model);
    if (is_set_str(src->tts.openai_voice)) snprintf(dst->tts.openai_voice, sizeof(dst->tts.openai_voice), "%s", src->tts.openai_voice);
    if (is_set_str(src->tts.xai_voice_id)) snprintf(dst->tts.xai_voice_id, sizeof(dst->tts.xai_voice_id), "%s", src->tts.xai_voice_id);
    if (is_set_str(src->tts.xai_language)) snprintf(dst->tts.xai_language, sizeof(dst->tts.xai_language), "%s", src->tts.xai_language);
    if (is_set_int(src->tts.xai_sample_rate)) dst->tts.xai_sample_rate = src->tts.xai_sample_rate;
    if (is_set_int(src->tts.xai_bit_rate)) dst->tts.xai_bit_rate = src->tts.xai_bit_rate;
    if (is_set_str(src->tts.mistral_model)) snprintf(dst->tts.mistral_model, sizeof(dst->tts.mistral_model), "%s", src->tts.mistral_model);
    if (is_set_str(src->tts.mistral_voice_id)) snprintf(dst->tts.mistral_voice_id, sizeof(dst->tts.mistral_voice_id), "%s", src->tts.mistral_voice_id);
    if (is_set_str(src->tts.neutts_ref_audio)) snprintf(dst->tts.neutts_ref_audio, sizeof(dst->tts.neutts_ref_audio), "%s", src->tts.neutts_ref_audio);
    if (is_set_str(src->tts.neutts_ref_text)) snprintf(dst->tts.neutts_ref_text, sizeof(dst->tts.neutts_ref_text), "%s", src->tts.neutts_ref_text);
    if (is_set_str(src->tts.neutts_model)) snprintf(dst->tts.neutts_model, sizeof(dst->tts.neutts_model), "%s", src->tts.neutts_model);
    if (is_set_str(src->tts.neutts_device)) snprintf(dst->tts.neutts_device, sizeof(dst->tts.neutts_device), "%s", src->tts.neutts_device);
    if (is_set_str(src->tts.piper_voice)) snprintf(dst->tts.piper_voice, sizeof(dst->tts.piper_voice), "%s", src->tts.piper_voice);

    /* STT merge */
    dst->stt.enabled = src->stt.enabled;
    if (is_set_str(src->stt.provider)) snprintf(dst->stt.provider, sizeof(dst->stt.provider), "%s", src->stt.provider);
    if (is_set_str(src->stt.local_model)) snprintf(dst->stt.local_model, sizeof(dst->stt.local_model), "%s", src->stt.local_model);
    if (is_set_str(src->stt.local_language)) snprintf(dst->stt.local_language, sizeof(dst->stt.local_language), "%s", src->stt.local_language);
    if (is_set_str(src->stt.local_command)) snprintf(dst->stt.local_command, sizeof(dst->stt.local_command), "%s", src->stt.local_command);
    if (is_set_str(src->stt.groq_model)) snprintf(dst->stt.groq_model, sizeof(dst->stt.groq_model), "%s", src->stt.groq_model);
    if (is_set_str(src->stt.openai_model)) snprintf(dst->stt.openai_model, sizeof(dst->stt.openai_model), "%s", src->stt.openai_model);
    if (is_set_str(src->stt.mistral_model)) snprintf(dst->stt.mistral_model, sizeof(dst->stt.mistral_model), "%s", src->stt.mistral_model);
    if (is_set_str(src->stt.xai_model)) snprintf(dst->stt.xai_model, sizeof(dst->stt.xai_model), "%s", src->stt.xai_model);
    if (is_set_str(src->stt.xai_language)) snprintf(dst->stt.xai_language, sizeof(dst->stt.xai_language), "%s", src->stt.xai_language);
    dst->stt.xai_format = src->stt.xai_format;
    dst->stt.xai_diarize = src->stt.xai_diarize;
    if (is_set_str(src->stt.elevenlabs_model)) snprintf(dst->stt.elevenlabs_model, sizeof(dst->stt.elevenlabs_model), "%s", src->stt.elevenlabs_model);
    if (is_set_str(src->stt.elevenlabs_language)) snprintf(dst->stt.elevenlabs_language, sizeof(dst->stt.elevenlabs_language), "%s", src->stt.elevenlabs_language);
    dst->stt.elevenlabs_tag_audio_events = src->stt.elevenlabs_tag_audio_events;
    dst->stt.elevenlabs_diarize = src->stt.elevenlabs_diarize;
    if (is_set_str(src->stt.deepgram_model)) snprintf(dst->stt.deepgram_model, sizeof(dst->stt.deepgram_model), "%s", src->stt.deepgram_model);
    if (is_set_str(src->stt.command_timeout)) snprintf(dst->stt.command_timeout, sizeof(dst->stt.command_timeout), "%s", src->stt.command_timeout);
    if (is_set_str(src->stt.command_format)) snprintf(dst->stt.command_format, sizeof(dst->stt.command_format), "%s", src->stt.command_format);

    /* Voice merge */
    if (is_set_str(src->voice.record_key)) snprintf(dst->voice.record_key, sizeof(dst->voice.record_key), "%s", src->voice.record_key);
    if (is_set_int(src->voice.max_recording_seconds)) dst->voice.max_recording_seconds = src->voice.max_recording_seconds;
    dst->voice.auto_tts = src->voice.auto_tts;
    dst->voice.beep_enabled = src->voice.beep_enabled;
    if (is_set_int(src->voice.silence_threshold)) dst->voice.silence_threshold = src->voice.silence_threshold;
    if (src->voice.silence_duration >= 0.1f) dst->voice.silence_duration = src->voice.silence_duration;

    /* Secrets merge */
    dst->secrets.enabled = src->secrets.enabled;
    if (is_set_str(src->secrets.access_token))
        snprintf(dst->secrets.access_token, sizeof(dst->secrets.access_token), "%s", src->secrets.access_token);
    if (is_set_str(src->secrets.organization_id))
        snprintf(dst->secrets.organization_id, sizeof(dst->secrets.organization_id), "%s", src->secrets.organization_id);
    if (is_set_str(src->secrets.bws_path))
        snprintf(dst->secrets.bws_path, sizeof(dst->secrets.bws_path), "%s", src->secrets.bws_path);
    if (is_set_int(src->secrets.install_timeout))
        dst->secrets.install_timeout = src->secrets.install_timeout;

    /* Sync flat fields for backward compat */
    snprintf(dst->model, sizeof(dst->model), "%s", dst->provider_cfg.model);
    snprintf(dst->provider, sizeof(dst->provider), "%s", dst->provider_cfg.provider);
    snprintf(dst->base_url, sizeof(dst->base_url), "%s", dst->provider_cfg.base_url);
    snprintf(dst->api_key, sizeof(dst->api_key), "%s", dst->provider_cfg.api_key);
    dst->max_turns = dst->agent.max_iterations;
    dst->verbose = dst->agent.verbose_level;
    dst->fast_mode = dst->agent.fast_mode;
    dst->quiet_mode = dst->agent.quiet_mode;
}
bool hermes_config_export(const hermes_config_t *cfg, const char *path) {
    FILE *f = stdout;
    bool close_file = false;
    if (path && path[0]) {
        f = fopen(path, "w");
        if (!f) { fprintf(stderr, "Error: cannot write %s\n", path); return false; }
        close_file = true;
    }

    fprintf(f, "# Hermes C Config Export\n\n");
    exp_int(f, "config_version", cfg->config_version > 0 ? cfg->config_version : HERMES_CONFIG_VERSION);

    fprintf(f, "\nmodel:\n");
    exp_str(f, "  default", cfg->provider_cfg.model);
    exp_str(f, "  provider", cfg->provider_cfg.provider);
    exp_str(f, "  base_url", cfg->provider_cfg.base_url);
    exp_str(f, "  api_mode", cfg->provider_cfg.api_mode);
    exp_str(f, "  fallback_model", cfg->provider_cfg.fallback_model);
    exp_str(f, "  fallback_providers", cfg->provider_cfg.fallback_providers);
    exp_str(f, "  service_tier", cfg->provider_cfg.service_tier);
    exp_int(f, "  max_tokens", cfg->provider_cfg.max_tokens);
    exp_float(f, "  temperature", cfg->provider_cfg.temperature);
    exp_float(f, "  top_p", cfg->provider_cfg.top_p);
    exp_str(f, "  response_format", cfg->provider_cfg.response_format);
    exp_str(f, "  metadata", cfg->provider_cfg.metadata);
    exp_str(f, "  tool_choice", cfg->provider_cfg.tool_choice);
    exp_bool(f, "  parallel_tool_calls", cfg->provider_cfg.parallel_tool_calls);
    exp_int(f, "  max_tool_calls", cfg->provider_cfg.max_tool_calls);
    exp_int(f, "  n", cfg->provider_cfg.n);
    exp_str(f, "  azure_deployment_id", cfg->provider_cfg.azure_deployment_id);
    exp_str(f, "  azure_api_version", cfg->provider_cfg.azure_api_version);
    exp_str(f, "  openrouter_provider", cfg->provider_cfg.openrouter_provider);
    exp_str(f, "  bedrock_inference_profile", cfg->provider_cfg.bedrock_inference_profile);
    exp_str(f, "  bedrock_guardrail_config", cfg->provider_cfg.bedrock_guardrail_config);
    exp_bool(f, "  bedrock_trace_enabled", cfg->provider_cfg.bedrock_trace_enabled);
    exp_bool(f, "  json_mode", cfg->provider_cfg.json_mode);
    exp_bool(f, "  response_format_strict", cfg->provider_cfg.response_format_strict);
    exp_bool(f, "  supports_vision", cfg->provider_cfg.supports_vision);
    exp_str(f, "  vision_overrides", cfg->provider_cfg.vision_overrides);

    fprintf(f, "\ndisplay:\n");
    exp_str(f, "  skin", cfg->display.skin);
    exp_str(f, "  banner", cfg->display.banner);
    exp_str(f, "  spinner", cfg->display.spinner_style);
    exp_str(f, "  indicator", cfg->display.indicator);
    exp_str(f, "  language", cfg->display.language);
    exp_str(f, "  personality", cfg->display.personality);
    exp_bool(f, "  streaming", cfg->display.stream);
    exp_bool(f, "  show_reasoning", cfg->display.show_reasoning);
    exp_bool(f, "  compact", cfg->display.compact);
    exp_bool(f, "  show_cost", cfg->display.show_cost);
    exp_bool(f, "  timestamps", cfg->display.timestamps);

    fprintf(f, "\nagent:\n");
    exp_int(f, "  max_turns", cfg->agent.max_iterations);
    exp_int(f, "  max_tool_calls_round", cfg->agent.max_tool_calls_round);
    exp_int(f, "  verbose", cfg->agent.verbose_level);
    exp_int(f, "  api_max_retries", cfg->agent.api_max_retries);
    exp_int(f, "  clarify_timeout", cfg->agent.clarify_timeout);
    exp_float(f, "  compress_threshold", cfg->agent.compress_threshold);
    exp_str(f, "  system_prompt", cfg->agent.system_prompt);
    exp_str(f, "  profile", cfg->agent.profile);
    exp_str(f, "  reasoning_effort", cfg->agent.reasoning_effort);
    exp_bool(f, "  fast", cfg->agent.fast_mode);
    exp_bool(f, "  quiet", cfg->agent.quiet_mode);

    fprintf(f, "\nterminal:\n");
    exp_int(f, "  timeout", cfg->tools.terminal_timeout);
    exp_str(f, "  backend", cfg->tools.terminal_backend);
    exp_bool(f, "  persistent_shell", cfg->tools.persistent_shell);

    fprintf(f, "\nweb:\n");
    exp_str(f, "  backend", cfg->tools.web_backend);
    exp_str(f, "  search_backend", cfg->tools.web_search_backend);
    exp_str(f, "  extract_backend", cfg->tools.web_extract_backend);
    exp_int(f, "  search_timeout", cfg->tools.web_search_timeout);

    fprintf(f, "\napprovals:\n");
    exp_str(f, "  mode", cfg->tools.approval_mode);
    exp_int(f, "  timeout", cfg->tools.approval_timeout);

    fprintf(f, "\ntool_output:\n");
    exp_int(f, "  max_bytes", cfg->tools.max_result_size);

    fprintf(f, "\ncompression:\n");
    exp_str(f, "  model", cfg->compression.model);
    exp_str(f, "  strategy", cfg->compression.strategy);
    exp_float(f, "  target_ratio", cfg->compression.target_ratio);
    exp_int(f, "  min_messages", cfg->compression.min_messages);
    exp_bool(f, "  preserve_system", cfg->compression.preserve_system);
    exp_int(f, "  protect_last_n", cfg->compression.protect_last_n);
    exp_int(f, "  protect_first_n", cfg->compression.protect_first_n);
    exp_int(f, "  hygiene_hard_message_limit", cfg->compression.hygiene_hard_message_limit);
    exp_int(f, "  cooldown_secs", cfg->compression.cooldown_secs);
    exp_int(f, "  failure_cooldown_secs", cfg->compression.failure_cooldown_secs);
    exp_bool(f, "  abort_on_summary_failure", cfg->compression.abort_on_summary_failure);
    exp_bool(f, "  enabled", cfg->compress_enabled);

    fprintf(f, "\ndelegation:\n");
    exp_int(f, "  max_concurrent_children", cfg->delegation.max_concurrent_children);
    exp_int(f, "  max_spawn_depth", cfg->delegation.max_spawn_depth);
    exp_int(f, "  child_timeout_seconds", cfg->delegation.child_timeout);
    exp_str(f, "  model", cfg->delegation.child_model);
    exp_str(f, "  provider", cfg->delegation.child_provider);
    exp_int(f, "  max_iterations", cfg->delegation.child_max_turns);

    fprintf(f, "\nbrowser:\n");
    exp_str(f, "  cdp_url", cfg->browser_cfg.cdp_url);
    exp_str(f, "  engine", cfg->browser_cfg.browser_type);
    exp_bool(f, "  headless", cfg->browser_cfg.headless);
    exp_int(f, "  command_timeout", cfg->browser_cfg.timeout);

    fprintf(f, "\nmemory:\n");
    exp_str(f, "  provider", cfg->memory.provider);
    exp_int(f, "  memory_char_limit", cfg->memory.char_limit);
    exp_int(f, "  user_char_limit", cfg->memory.user_char_limit);

    fprintf(f, "\nsecurity:\n");
    exp_str(f, "  tirith_path", cfg->security.tirith_path);
    exp_int(f, "  tirith_timeout", cfg->security.tirith_timeout);
    exp_bool(f, "  tirith_enabled", cfg->security.tirith_enabled);
    exp_bool(f, "  allow_private_urls", cfg->security.allow_private_urls);

    fprintf(f, "\nsessions:\n");
    exp_int(f, "  retention_days", cfg->session.retention_days);

    fprintf(f, "\nmcp:\n");
    exp_int(f, "  timeout", cfg->mcp.timeout);
    exp_bool(f, "  auth_enabled", cfg->mcp.auth_enabled);

    fprintf(f, "\nterminal:\n");
    exp_str(f, "  backend", cfg->terminal.backend);
    exp_int(f, "  timeout", cfg->terminal.timeout);
    exp_bool(f, "  persistent_shell", cfg->terminal.persistent_shell);
    exp_str(f, "  cwd", cfg->terminal.cwd);
    exp_str(f, "  env_passthrough", cfg->terminal.env_passthrough);
    exp_str(f, "  docker_image", cfg->terminal.docker_image);
    exp_str(f, "  docker_forward_env", cfg->terminal.docker_forward_env);
    exp_str(f, "  singularity_image", cfg->terminal.singularity_image);
    exp_str(f, "  modal_image", cfg->terminal.modal_image);
    exp_int(f, "  container_cpu", cfg->terminal.container_cpu);
    exp_int(f, "  container_memory", cfg->terminal.container_memory);
    exp_int(f, "  container_disk", cfg->terminal.container_disk);
    exp_bool(f, "  container_persistent", cfg->terminal.container_persistent);

    fprintf(f, "\nlogging:\n");
    exp_str(f, "  level", cfg->logging.level);
    exp_str(f, "  format", cfg->logging.format);
    exp_str(f, "  dir", cfg->logging.dir);
    exp_int(f, "  max_files", cfg->logging.max_files);
    exp_int(f, "  max_size_mb", cfg->logging.max_size_mb);

    fprintf(f, "\nskills:\n");
    exp_str(f, "  dir", cfg->skills.dir);
    exp_str(f, "  enabled", cfg->skills.enabled);
    exp_bool(f, "  auto_discover", cfg->skills.auto_discover);
    exp_int(f, "  bundle_size_limit", cfg->skills.bundle_size_limit);
    exp_int(f, "  validate", cfg->skills.validate_on_load);

    fprintf(f, "\ncheckpoints:\n");
    exp_bool(f, "  enabled", cfg->checkpoints.enabled);
    exp_int(f, "  interval", cfg->checkpoints.interval);
    exp_int(f, "  max", cfg->checkpoints.max_checkpoints);
    exp_str(f, "  dir", cfg->checkpoints.dir);

    fprintf(f, "\nsecrets:\n  bitwarden:\n");
    exp_bool(f, "    enabled", cfg->secrets.enabled);
    exp_str(f, "    access_token", cfg->secrets.access_token);
    exp_str(f, "    organization_id", cfg->secrets.organization_id);
    exp_str(f, "    bws_path", cfg->secrets.bws_path);
    exp_int(f, "    install_timeout", cfg->secrets.install_timeout);

    /* Auxiliary export */
    #define E_AUX_TASK(f, task, nm) do {         fprintf(f, "\nauxiliary." #task ":\n");         exp_str(f, "  provider", cfg->auxiliary.task.provider);         exp_str(f, "  model", cfg->auxiliary.task.model);         exp_str(f, "  base_url", cfg->auxiliary.task.base_url);         exp_str(f, "  api_key", cfg->auxiliary.task.api_key);         exp_int(f, "  timeout", cfg->auxiliary.task.timeout);         exp_str(f, "  extra_body", cfg->auxiliary.task.extra_body);     } while(0)
    E_AUX_TASK(f, vision, "vision");
    exp_int(f, "  download_timeout", cfg->auxiliary.vision_download_timeout);
    E_AUX_TASK(f, web_extract, "web_extract");
    E_AUX_TASK(f, compression, "compression");
    E_AUX_TASK(f, skills_hub, "skills_hub");
    E_AUX_TASK(f, approval, "approval");
    E_AUX_TASK(f, mcp, "mcp");
    E_AUX_TASK(f, title_generation, "title_generation");
    E_AUX_TASK(f, triage_specifier, "triage_specifier");
    E_AUX_TASK(f, kanban_decomposer, "kanban_decomposer");
    E_AUX_TASK(f, profile_describer, "profile_describer");
    E_AUX_TASK(f, curator, "curator");
    #undef E_AUX_TASK

    fprintf(f, "\ntts:\n");
    exp_str(f, "  provider", cfg->tts.provider);
    exp_str(f, "  edge.voice", cfg->tts.edge_voice);
    exp_str(f, "  elevenlabs.voice_id", cfg->tts.elevenlabs_voice_id);
    exp_str(f, "  elevenlabs.model_id", cfg->tts.elevenlabs_model_id);
    exp_str(f, "  openai.model", cfg->tts.openai_model);
    exp_str(f, "  openai.voice", cfg->tts.openai_voice);
    exp_str(f, "  xai.voice_id", cfg->tts.xai_voice_id);
    exp_str(f, "  xai.language", cfg->tts.xai_language);
    exp_int(f, "  xai.sample_rate", cfg->tts.xai_sample_rate);
    exp_int(f, "  xai.bit_rate", cfg->tts.xai_bit_rate);
    exp_str(f, "  mistral.model", cfg->tts.mistral_model);
    exp_str(f, "  mistral.voice_id", cfg->tts.mistral_voice_id);
    exp_str(f, "  neutts.ref_audio", cfg->tts.neutts_ref_audio);
    exp_str(f, "  neutts.ref_text", cfg->tts.neutts_ref_text);
    exp_str(f, "  neutts.model", cfg->tts.neutts_model);
    exp_str(f, "  neutts.device", cfg->tts.neutts_device);
    exp_str(f, "  piper.voice", cfg->tts.piper_voice);

    fprintf(f, "\nstt:\n");
    exp_bool(f, "  enabled", cfg->stt.enabled);
    exp_str(f, "  provider", cfg->stt.provider);
    exp_str(f, "  local.model", cfg->stt.local_model);
    exp_str(f, "  local.language", cfg->stt.local_language);
    exp_str(f, "  local.command", cfg->stt.local_command);
    exp_str(f, "  groq.model", cfg->stt.groq_model);
    exp_str(f, "  openai.model", cfg->stt.openai_model);
    exp_str(f, "  mistral.model", cfg->stt.mistral_model);
    exp_str(f, "  xai.model", cfg->stt.xai_model);
    exp_str(f, "  xai.language", cfg->stt.xai_language);
    exp_bool(f, "  xai.format", cfg->stt.xai_format);
    exp_bool(f, "  xai.diarize", cfg->stt.xai_diarize);
    exp_str(f, "  elevenlabs.model_id", cfg->stt.elevenlabs_model);
    exp_str(f, "  elevenlabs.language_code", cfg->stt.elevenlabs_language);
    exp_bool(f, "  elevenlabs.tag_audio_events", cfg->stt.elevenlabs_tag_audio_events);
    exp_bool(f, "  elevenlabs.diarize", cfg->stt.elevenlabs_diarize);
    exp_str(f, "  deepgram.model", cfg->stt.deepgram_model);
    exp_str(f, "  command.timeout_seconds", cfg->stt.command_timeout);
    exp_str(f, "  command.format", cfg->stt.command_format);

    fprintf(f, "\nvoice:\n");
    exp_str(f, "  record_key", cfg->voice.record_key);
    exp_int(f, "  max_recording_seconds", cfg->voice.max_recording_seconds);
    exp_bool(f, "  auto_tts", cfg->voice.auto_tts);
    exp_bool(f, "  beep_enabled", cfg->voice.beep_enabled);
    exp_int(f, "  silence_threshold", cfg->voice.silence_threshold);
    exp_float(f, "  silence_duration", cfg->voice.silence_duration);

    if (cfg->gateway_platforms[0]) {
        fprintf(f, "\ngateway:\n");
        exp_str(f, "  platforms", cfg->gateway_platforms);
    }

    if (close_file) fclose(f);
    return true;
}
bool hermes_config_import(hermes_config_t *cfg, const char *path) {
    if (!path || !path[0]) return false;

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (!doc) {
        if (err) { fprintf(stderr, "Import error: %s\n", err); free(err); }
        return false;
    }

    /* Merge imported values over current config */
    const char *v;
    v = yaml_get_string(doc, "model.default");
    if (v) snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", v);
    v = yaml_get_string(doc, "model.provider");
    if (v) snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", v);
    v = yaml_get_string(doc, "model.base_url");
    if (v) snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", v);
    v = yaml_get_string(doc, "model.api_key");
    if (v) snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", v);

    int iv;
    iv = yaml_get_int(doc, "agent.max_turns", 0);
    if (iv > 0) { cfg->agent.max_iterations = iv; cfg->max_turns = iv; }

    v = yaml_get_string(doc, "display.skin");
    if (v) snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);

    v = yaml_get_string(doc, "approvals.mode");
    if (v) snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "%s", v);

    yaml_free(doc);
    return true;
}
