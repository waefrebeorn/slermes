/*
 * config_diff.c -- extracted from cli/config.c monolith.
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
#include "hermes_provider_xai.h"
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

static void diff_str(cfg_diff_t *d, const char *key,
                     const char *def, const char *act) {
    if (d->count >= 128) return;
    if (def[0] == '\0' && act[0] != '\0') {
        d->entries[d->count].type = CFG_DIFF_ADDED;
    } else if (def[0] != '\0' && act[0] == '\0') {
        d->entries[d->count].type = CFG_DIFF_MISSING;
    } else if (strcmp(def, act) != 0) {
        d->entries[d->count].type = CFG_DIFF_CHANGED;
    } else {
        return; /* identical, skip */
    }
    snprintf(d->entries[d->count].key, sizeof(d->entries[d->count].key), "%s", key);
    snprintf(d->entries[d->count].default_value, sizeof(d->entries[d->count].default_value), "%s", def);
    snprintf(d->entries[d->count].active_value, sizeof(d->entries[d->count].active_value), "%s", act);
    d->count++;
}
static void diff_int(cfg_diff_t *d, const char *key, int def, int act) {
    char dbuf[32], abuf[32];
    snprintf(dbuf, sizeof(dbuf), "%d", def);
    snprintf(abuf, sizeof(abuf), "%d", act);
    diff_str(d, key, dbuf, abuf);
}
static void diff_bool(cfg_diff_t *d, const char *key, bool def, bool act) {
    diff_str(d, key, def ? "true" : "false", act ? "true" : "false");
}
static void diff_float(cfg_diff_t *d, const char *key, float def, float act) {
    char dbuf[32], abuf[32];
    snprintf(dbuf, sizeof(dbuf), "%.2f", (double)def);
    snprintf(abuf, sizeof(abuf), "%.2f", (double)act);
    diff_str(d, key, dbuf, abuf);
}
bool hermes_config_diff(const hermes_config_t *active, cfg_diff_t *diff) {
    memset(diff, 0, sizeof(*diff));
    hermes_config_t def;
    hermes_config_defaults(&def);

    /* Provider group */
    diff_str(diff, "model.default", def.provider_cfg.model, active->provider_cfg.model);
    diff_str(diff, "model.provider", def.provider_cfg.provider, active->provider_cfg.provider);
    diff_str(diff, "model.base_url", def.provider_cfg.base_url, active->provider_cfg.base_url);
    diff_str(diff, "model.api_mode", def.provider_cfg.api_mode, active->provider_cfg.api_mode);
    diff_str(diff, "model.fallback_model", def.provider_cfg.fallback_model, active->provider_cfg.fallback_model);
    diff_str(diff, "model.fallback_providers", def.provider_cfg.fallback_providers, active->provider_cfg.fallback_providers);
    diff_str(diff, "model.service_tier", def.provider_cfg.service_tier, active->provider_cfg.service_tier);
    diff_int(diff, "model.max_tokens", def.provider_cfg.max_tokens, active->provider_cfg.max_tokens);
    diff_float(diff, "model.temperature", def.provider_cfg.temperature, active->provider_cfg.temperature);
    diff_float(diff, "model.top_p", def.provider_cfg.top_p, active->provider_cfg.top_p);
    diff_float(diff, "model.presence_penalty", def.provider_cfg.presence_penalty, active->provider_cfg.presence_penalty);
    diff_float(diff, "model.frequency_penalty", def.provider_cfg.frequency_penalty, active->provider_cfg.frequency_penalty);
    diff_int(diff, "model.seed", def.provider_cfg.seed, active->provider_cfg.seed);
    diff_bool(diff, "model.logprobs", def.provider_cfg.logprobs, active->provider_cfg.logprobs);
    diff_int(diff, "model.top_logprobs", def.provider_cfg.top_logprobs, active->provider_cfg.top_logprobs);
    diff_str(diff, "model.user", def.provider_cfg.user, active->provider_cfg.user);
    diff_str(diff, "model.response_format", def.provider_cfg.response_format, active->provider_cfg.response_format);
    diff_str(diff, "model.metadata", def.provider_cfg.metadata, active->provider_cfg.metadata);
    diff_str(diff, "model.tool_choice", def.provider_cfg.tool_choice, active->provider_cfg.tool_choice);
    diff_bool(diff, "model.parallel_tool_calls", def.provider_cfg.parallel_tool_calls, active->provider_cfg.parallel_tool_calls);
    diff_int(diff, "model.max_tool_calls", def.provider_cfg.max_tool_calls, active->provider_cfg.max_tool_calls);
    diff_int(diff, "model.n", def.provider_cfg.n, active->provider_cfg.n);
    diff_int(diff, "model.top_k", def.provider_cfg.top_k, active->provider_cfg.top_k);
    diff_int(diff, "model.candidate_count", def.provider_cfg.candidate_count, active->provider_cfg.candidate_count);
    diff_bool(diff, "model.json_mode", def.provider_cfg.json_mode, active->provider_cfg.json_mode);
    diff_bool(diff, "model.response_format_strict", def.provider_cfg.response_format_strict, active->provider_cfg.response_format_strict);
    diff_str(diff, "model.safety_settings", def.provider_cfg.safety_settings, active->provider_cfg.safety_settings);
    diff_str(diff, "azure.deployment_id", def.provider_cfg.azure_deployment_id, active->provider_cfg.azure_deployment_id);
    diff_str(diff, "azure.api_version", def.provider_cfg.azure_api_version, active->provider_cfg.azure_api_version);
    diff_str(diff, "openrouter.provider", def.provider_cfg.openrouter_provider, active->provider_cfg.openrouter_provider);
    diff_str(diff, "bedrock.inference_profile", def.provider_cfg.bedrock_inference_profile, active->provider_cfg.bedrock_inference_profile);
    diff_str(diff, "bedrock.guardrail_config", def.provider_cfg.bedrock_guardrail_config, active->provider_cfg.bedrock_guardrail_config);
    diff_bool(diff, "bedrock.trace_enabled", def.provider_cfg.bedrock_trace_enabled, active->provider_cfg.bedrock_trace_enabled);
    diff_bool(diff, "model.supports_vision", def.provider_cfg.supports_vision, active->provider_cfg.supports_vision);
    diff_str(diff, "model.vision_overrides", def.provider_cfg.vision_overrides, active->provider_cfg.vision_overrides);

    /* Display group */
    diff_str(diff, "display.skin", def.display.skin, active->display.skin);
    diff_str(diff, "display.banner", def.display.banner, active->display.banner);
    diff_str(diff, "display.spinner_style", def.display.spinner_style, active->display.spinner_style);
    diff_str(diff, "display.indicator", def.display.indicator, active->display.indicator);
    diff_str(diff, "display.language", def.display.language, active->display.language);
    diff_bool(diff, "display.streaming", def.display.stream, active->display.stream);
    diff_bool(diff, "display.show_reasoning", def.display.show_reasoning, active->display.show_reasoning);
    diff_bool(diff, "display.compact", def.display.compact, active->display.compact);

    /* Agent group */
    diff_int(diff, "agent.max_turns", def.agent.max_iterations, active->agent.max_iterations);
    diff_int(diff, "agent.max_tool_calls_round", def.agent.max_tool_calls_round, active->agent.max_tool_calls_round);
    diff_int(diff, "agent.verbose", def.agent.verbose_level, active->agent.verbose_level);
    diff_int(diff, "agent.api_max_retries", def.agent.api_max_retries, active->agent.api_max_retries);
    diff_int(diff, "agent.clarify_timeout", def.agent.clarify_timeout, active->agent.clarify_timeout);
    diff_float(diff, "compression.threshold", def.agent.compress_threshold, active->agent.compress_threshold);
    diff_int(diff, "compression.protect_last_n", def.compression.protect_last_n, active->compression.protect_last_n);
    diff_int(diff, "compression.protect_first_n", def.compression.protect_first_n, active->compression.protect_first_n);
    diff_int(diff, "compression.hygiene_hard_message_limit", def.compression.hygiene_hard_message_limit, active->compression.hygiene_hard_message_limit);
    diff_int(diff, "compression.cooldown_secs", def.compression.cooldown_secs, active->compression.cooldown_secs);
    diff_int(diff, "compression.failure_cooldown_secs", def.compression.failure_cooldown_secs, active->compression.failure_cooldown_secs);
    diff_str(diff, "agent.system_prompt", def.agent.system_prompt, active->agent.system_prompt);
    diff_str(diff, "agent.profile", def.agent.profile, active->agent.profile);

    /* Tools group */
    diff_str(diff, "approvals.mode", def.tools.approval_mode, active->tools.approval_mode);
    diff_int(diff, "approvals.timeout", def.tools.approval_timeout, active->tools.approval_timeout);
    diff_int(diff, "tool_output.max_bytes", def.tools.max_result_size, active->tools.max_result_size);
    diff_int(diff, "terminal.timeout", def.tools.terminal_timeout, active->tools.terminal_timeout);
    /* Auxiliary group — diff all 11 sub-tasks */
    #define D_AUX_STR(task, field) diff_str(diff, "auxiliary." #task "." #field, def.auxiliary.task.field, active->auxiliary.task.field)
    #define D_AUX_INT(task, field) diff_int(diff, "auxiliary." #task "." #field, def.auxiliary.task.field, active->auxiliary.task.field)
    D_AUX_STR(vision, provider); D_AUX_STR(vision, model); D_AUX_STR(vision, base_url);
    D_AUX_STR(vision, api_key); D_AUX_INT(vision, timeout); D_AUX_STR(vision, extra_body);
    diff_int(diff, "auxiliary.vision.download_timeout", def.auxiliary.vision_download_timeout, active->auxiliary.vision_download_timeout);
    D_AUX_STR(web_extract, provider); D_AUX_STR(web_extract, model); D_AUX_STR(web_extract, base_url);
    D_AUX_STR(web_extract, api_key); D_AUX_INT(web_extract, timeout); D_AUX_STR(web_extract, extra_body);
    D_AUX_STR(compression, provider); D_AUX_STR(compression, model); D_AUX_STR(compression, base_url);
    D_AUX_STR(compression, api_key); D_AUX_INT(compression, timeout); D_AUX_STR(compression, extra_body);
    D_AUX_STR(skills_hub, provider); D_AUX_STR(skills_hub, model); D_AUX_STR(skills_hub, base_url);
    D_AUX_STR(skills_hub, api_key); D_AUX_INT(skills_hub, timeout); D_AUX_STR(skills_hub, extra_body);
    D_AUX_STR(approval, provider); D_AUX_STR(approval, model); D_AUX_STR(approval, base_url);
    D_AUX_STR(approval, api_key); D_AUX_INT(approval, timeout); D_AUX_STR(approval, extra_body);
    D_AUX_STR(mcp, provider); D_AUX_STR(mcp, model); D_AUX_STR(mcp, base_url);
    D_AUX_STR(mcp, api_key); D_AUX_INT(mcp, timeout); D_AUX_STR(mcp, extra_body);
    D_AUX_STR(title_generation, provider); D_AUX_STR(title_generation, model); D_AUX_STR(title_generation, base_url);
    D_AUX_STR(title_generation, api_key); D_AUX_INT(title_generation, timeout); D_AUX_STR(title_generation, extra_body);
    D_AUX_STR(triage_specifier, provider); D_AUX_STR(triage_specifier, model); D_AUX_STR(triage_specifier, base_url);
    D_AUX_STR(triage_specifier, api_key); D_AUX_INT(triage_specifier, timeout); D_AUX_STR(triage_specifier, extra_body);
    D_AUX_STR(kanban_decomposer, provider); D_AUX_STR(kanban_decomposer, model); D_AUX_STR(kanban_decomposer, base_url);
    D_AUX_STR(kanban_decomposer, api_key); D_AUX_INT(kanban_decomposer, timeout); D_AUX_STR(kanban_decomposer, extra_body);
    D_AUX_STR(profile_describer, provider); D_AUX_STR(profile_describer, model); D_AUX_STR(profile_describer, base_url);
    D_AUX_STR(profile_describer, api_key); D_AUX_INT(profile_describer, timeout); D_AUX_STR(profile_describer, extra_body);
    D_AUX_STR(curator, provider); D_AUX_STR(curator, model); D_AUX_STR(curator, base_url);
    D_AUX_STR(curator, api_key); D_AUX_INT(curator, timeout); D_AUX_STR(curator, extra_body);
    #undef D_AUX_STR
    #undef D_AUX_INT
    diff_str(diff, "terminal.backend", def.tools.terminal_backend, active->tools.terminal_backend);
    diff_bool(diff, "terminal.persistent_shell", def.tools.persistent_shell, active->tools.persistent_shell);
    diff_str(diff, "web.backend", def.tools.web_backend, active->tools.web_backend);
    diff_str(diff, "web.search_backend", def.tools.web_search_backend, active->tools.web_search_backend);
    diff_str(diff, "web.extract_backend", def.tools.web_extract_backend, active->tools.web_extract_backend);
    diff_int(diff, "web.search_timeout", def.tools.web_search_timeout, active->tools.web_search_timeout);
    diff_str(diff, "tools.enabled_toolsets", def.tools.enabled_toolsets, active->tools.enabled_toolsets);
    diff_str(diff, "tools.disabled_toolsets", def.tools.disabled_toolsets, active->tools.disabled_toolsets);
    diff_str(diff, "tools.environments", def.tools.environments, active->tools.environments);

    /* Delegation */
    diff_int(diff, "delegation.max_concurrent_children",
             def.delegation.max_concurrent_children, active->delegation.max_concurrent_children);
    diff_int(diff, "delegation.max_spawn_depth",
             def.delegation.max_spawn_depth, active->delegation.max_spawn_depth);
    diff_str(diff, "delegation.model", def.delegation.child_model, active->delegation.child_model);

    /* Security */
    diff_bool(diff, "security.tirith_enabled", def.security.tirith_enabled, active->security.tirith_enabled);

    /* Session */
    diff_int(diff, "sessions.retention_days", def.session.retention_days, active->session.retention_days);

    /* Terminal (expanded) */
    diff_str(diff, "terminal.backend", def.terminal.backend, active->terminal.backend);
    diff_int(diff, "terminal.timeout", def.terminal.timeout, active->terminal.timeout);
    diff_str(diff, "terminal.cwd", def.terminal.cwd, active->terminal.cwd);
    diff_str(diff, "terminal.env_passthrough", def.terminal.env_passthrough, active->terminal.env_passthrough);
    diff_bool(diff, "terminal.auto_source_bashrc", def.terminal.auto_source_bashrc, active->terminal.auto_source_bashrc);
    diff_str(diff, "terminal.docker_image", def.terminal.docker_image, active->terminal.docker_image);

    /* Logging */
    diff_str(diff, "logging.level", def.logging.level, active->logging.level);
    diff_str(diff, "logging.format", def.logging.format, active->logging.format);
    diff_int(diff, "logging.max_files", def.logging.max_files, active->logging.max_files);
    diff_int(diff, "logging.max_size_mb", def.logging.max_size_mb, active->logging.max_size_mb);

    /* Skills */
    diff_bool(diff, "skills.auto_discover", def.skills.auto_discover, active->skills.auto_discover);
    diff_int(diff, "skills.validate", def.skills.validate_on_load, active->skills.validate_on_load);

    /* Checkpoints */
    diff_bool(diff, "checkpoints.enabled", def.checkpoints.enabled, active->checkpoints.enabled);
    diff_int(diff, "checkpoints.interval", def.checkpoints.interval, active->checkpoints.interval);
    diff_int(diff, "checkpoints.max", def.checkpoints.max_checkpoints, active->checkpoints.max_checkpoints);

    /* Secrets */
    diff_bool(diff, "secrets.bitwarden.enabled", def.secrets.enabled, active->secrets.enabled);
    diff_str(diff, "secrets.bitwarden.access_token",
             def.secrets.access_token[0] ? "***set***" : "",
             active->secrets.access_token[0] ? "***set***" : "");

    /* TTS */
    diff_str(diff, "tts.provider", def.tts.provider, active->tts.provider);
    diff_str(diff, "tts.edge.voice", def.tts.edge_voice, active->tts.edge_voice);
    diff_str(diff, "tts.openai.model", def.tts.openai_model, active->tts.openai_model);
    diff_str(diff, "tts.openai.voice", def.tts.openai_voice, active->tts.openai_voice);
    diff_str(diff, "tts.piper.voice", def.tts.piper_voice, active->tts.piper_voice);

    /* STT */
    diff_bool(diff, "stt.enabled", def.stt.enabled, active->stt.enabled);
    diff_str(diff, "stt.provider", def.stt.provider, active->stt.provider);
    diff_str(diff, "stt.local.model", def.stt.local_model, active->stt.local_model);
    diff_str(diff, "stt.local.language", def.stt.local_language, active->stt.local_language);
    diff_str(diff, "stt.local.command", def.stt.local_command, active->stt.local_command);
    diff_str(diff, "stt.groq.model", def.stt.groq_model, active->stt.groq_model);
    diff_str(diff, "stt.openai.model", def.stt.openai_model, active->stt.openai_model);
    diff_str(diff, "stt.mistral.model", def.stt.mistral_model, active->stt.mistral_model);
    diff_str(diff, "stt.xai.model", def.stt.xai_model, active->stt.xai_model);
    diff_str(diff, "stt.xai.language", def.stt.xai_language, active->stt.xai_language);
    diff_bool(diff, "stt.xai.format", def.stt.xai_format, active->stt.xai_format);
    diff_bool(diff, "stt.xai.diarize", def.stt.xai_diarize, active->stt.xai_diarize);
    diff_str(diff, "stt.elevenlabs.model_id", def.stt.elevenlabs_model, active->stt.elevenlabs_model);
    diff_str(diff, "stt.elevenlabs.language_code", def.stt.elevenlabs_language, active->stt.elevenlabs_language);
    diff_bool(diff, "stt.elevenlabs.tag_audio_events", def.stt.elevenlabs_tag_audio_events, active->stt.elevenlabs_tag_audio_events);
    diff_bool(diff, "stt.elevenlabs.diarize", def.stt.elevenlabs_diarize, active->stt.elevenlabs_diarize);
    diff_str(diff, "stt.deepgram.model", def.stt.deepgram_model, active->stt.deepgram_model);
    diff_str(diff, "stt.command.timeout_seconds", def.stt.command_timeout, active->stt.command_timeout);
    diff_str(diff, "stt.command.format", def.stt.command_format, active->stt.command_format);

    /* Voice */
    diff_str(diff, "voice.record_key", def.voice.record_key, active->voice.record_key);
    diff_int(diff, "voice.max_recording_seconds", def.voice.max_recording_seconds, active->voice.max_recording_seconds);
    diff_bool(diff, "voice.auto_tts", def.voice.auto_tts, active->voice.auto_tts);

    return diff->count > 0;
}
static void add_issue(config_validation_t *r, const char *key, const char *fmt, ...) {
    if (!r || r->count >= 64) return;
    snprintf(r->issues[r->count].key, sizeof(r->issues[r->count].key), "%s", key);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->issues[r->count].message, sizeof(r->issues[r->count].message), fmt, ap);
    va_end(ap);
    r->count++;
}
bool hermes_config_validate(const hermes_config_t *cfg, config_validation_t *result) {
    if (!cfg) return false;
    if (result) memset(result, 0, sizeof(*result));

    /* --- Provider group --- */
    if (cfg->provider_cfg.model[0] == '\0')
        add_issue(result, "model.default", "model name is empty");
    if (cfg->provider_cfg.provider[0] == '\0')
        add_issue(result, "model.provider", "provider name is empty");
    if (cfg->provider_cfg.temperature < 0.0f || cfg->provider_cfg.temperature > 2.0f)
        add_issue(result, "model.temperature", "must be 0.0-2.0, got %.1f", cfg->provider_cfg.temperature);
    if (cfg->provider_cfg.top_p < 0.0f || cfg->provider_cfg.top_p > 1.0f)
        add_issue(result, "model.top_p", "must be 0.0-1.0, got %.2f", cfg->provider_cfg.top_p);
    if (cfg->provider_cfg.max_tokens < 1 || cfg->provider_cfg.max_tokens > 1048576)
        add_issue(result, "model.max_tokens", "unreasonable value %d", cfg->provider_cfg.max_tokens);
    if (cfg->provider_cfg.presence_penalty < -2.0f || cfg->provider_cfg.presence_penalty > 2.0f)
        add_issue(result, "model.presence_penalty", "must be -2.0-2.0, got %.1f", cfg->provider_cfg.presence_penalty);
    if (cfg->provider_cfg.frequency_penalty < -2.0f || cfg->provider_cfg.frequency_penalty > 2.0f)
        add_issue(result, "model.frequency_penalty", "must be -2.0-2.0, got %.1f", cfg->provider_cfg.frequency_penalty);
    if (cfg->provider_cfg.top_logprobs < 0 || cfg->provider_cfg.top_logprobs > 20)
        add_issue(result, "model.top_logprobs", "must be 0-20, got %d", cfg->provider_cfg.top_logprobs);

    /* L04: Check each model slot for xAI retirement */
    {
        char repl[128], reff[64];
        if (xai_is_model_retired(cfg->provider_cfg.model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "model.default", "xAI model '%s' retired May 15, 2026 → use '%s'%s",
                      cfg->provider_cfg.model, repl,
                      reff[0] ? reff : "");
        if (cfg->tools.vision_model[0] &&
            xai_is_model_retired(cfg->tools.vision_model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "tools.vision_model",
                      "xAI model '%s' retired → use '%s'", cfg->tools.vision_model, repl);
        if (cfg->delegation.child_model[0] &&
            xai_is_model_retired(cfg->delegation.child_model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "delegation.model",
                      "xAI model '%s' retired → use '%s'", cfg->delegation.child_model, repl);
        if (cfg->compression.model[0] &&
            xai_is_model_retired(cfg->compression.model, repl, sizeof(repl), reff, sizeof(reff)))
            add_issue(result, "compression.model",
                      "xAI model '%s' retired → use '%s'", cfg->compression.model, repl);
    }

    /* Validate api_mode enum */
    if (cfg->provider_cfg.api_mode[0] &&
        strcmp(cfg->provider_cfg.api_mode, "chat_completions") != 0 &&
        strcmp(cfg->provider_cfg.api_mode, "codex_responses") != 0)
        add_issue(result, "model.api_mode", "unknown mode '%s'", cfg->provider_cfg.api_mode);

    /* Validate reasoning_effort enum */
    if (cfg->provider_cfg.reasoning_effort[0] &&
        strcmp(cfg->provider_cfg.reasoning_effort, "low") != 0 &&
        strcmp(cfg->provider_cfg.reasoning_effort, "medium") != 0 &&
        strcmp(cfg->provider_cfg.reasoning_effort, "high") != 0)
        add_issue(result, "model.reasoning_effort", "unknown '%s' (low/medium/high)", cfg->provider_cfg.reasoning_effort);

    /* --- Display group --- */
    if (cfg->display.language[0] &&
        strcmp(cfg->display.language, "en") != 0 &&
        strcmp(cfg->display.language, "zh") != 0 &&
        strcmp(cfg->display.language, "ja") != 0)
        add_issue(result, "display.language", "unsupported '%s'", cfg->display.language);

    /* --- Agent group --- */
    if (cfg->agent.max_iterations < 1 || cfg->agent.max_iterations > 10000)
        add_issue(result, "agent.max_turns", "unreasonable %d", cfg->agent.max_iterations);
    if (cfg->agent.max_tool_calls_round < 0 || cfg->agent.max_tool_calls_round > 1000)
        add_issue(result, "agent.max_tool_calls_round", "unreasonable %d", cfg->agent.max_tool_calls_round);
    if (cfg->agent.verbose_level < 0 || cfg->agent.verbose_level > 2)
        add_issue(result, "agent.verbose", "must be 0-2, got %d", cfg->agent.verbose_level);
    if (cfg->agent.compress_threshold < 0.0f || cfg->agent.compress_threshold > 1.0f)
        add_issue(result, "compression.threshold", "must be 0.0-1.0, got %.2f", cfg->agent.compress_threshold);
    if (cfg->agent.api_max_retries < 0 || cfg->agent.api_max_retries > 100)
        add_issue(result, "agent.api_max_retries", "unreasonable %d", cfg->agent.api_max_retries);

    /* --- Tools group --- */
    if (cfg->tools.approval_mode[0] &&
        strcmp(cfg->tools.approval_mode, "off") != 0 &&
        strcmp(cfg->tools.approval_mode, "manual") != 0 &&
        strcmp(cfg->tools.approval_mode, "auto") != 0)
        add_issue(result, "approvals.mode", "unknown '%s'", cfg->tools.approval_mode);
    if (cfg->tools.approval_timeout < 0 || cfg->tools.approval_timeout > 86400)
        add_issue(result, "approvals.timeout", "unreasonable %d", cfg->tools.approval_timeout);
    if (cfg->tools.max_result_size < 256)
        add_issue(result, "tool_output.max_bytes", "too small %d, min 256", cfg->tools.max_result_size);
    if (cfg->tools.terminal_timeout < 1 || cfg->tools.terminal_timeout > 86400)
        add_issue(result, "terminal.timeout", "unreasonable %d", cfg->tools.terminal_timeout);

    /* --- Delegation --- */
    if (cfg->delegation.max_concurrent_children < 1 || cfg->delegation.max_concurrent_children > 50)
        add_issue(result, "delegation.max_concurrent_children", "unreasonable %d", cfg->delegation.max_concurrent_children);
    if (cfg->delegation.max_spawn_depth < 0 || cfg->delegation.max_spawn_depth > 10)
        add_issue(result, "delegation.max_spawn_depth", "unreasonable %d", cfg->delegation.max_spawn_depth);

    /* --- Security --- */
    if (cfg->security.tirith_timeout < 0 || cfg->security.tirith_timeout > 300)
        add_issue(result, "security.tirith_timeout", "unreasonable %d", cfg->security.tirith_timeout);

    /* --- Session --- */
    if (cfg->session.retention_days < 0 || cfg->session.retention_days > 3650)
        add_issue(result, "sessions.retention_days", "unreasonable %d", cfg->session.retention_days);

    /* --- Browser (P6) --- */
    if (cfg->browser_cfg.browser_type[0] &&
        strcmp(cfg->browser_cfg.browser_type, "auto") != 0 &&
        strcmp(cfg->browser_cfg.browser_type, "chromium") != 0 &&
        strcmp(cfg->browser_cfg.browser_type, "firefox") != 0)
        add_issue(result, "browser.engine", "unknown '%s' (auto/chromium/firefox)", cfg->browser_cfg.browser_type);
    if (cfg->browser_cfg.viewport_width < 320 || cfg->browser_cfg.viewport_width > 7680)
        add_issue(result, "browser.viewport_width", "unreasonable %d", cfg->browser_cfg.viewport_width);
    if (cfg->browser_cfg.viewport_height < 240 || cfg->browser_cfg.viewport_height > 4320)
        add_issue(result, "browser.viewport_height", "unreasonable %d", cfg->browser_cfg.viewport_height);
    if (cfg->browser_cfg.timeout < 1 || cfg->browser_cfg.timeout > 300)
        add_issue(result, "browser.timeout", "unreasonable %d", cfg->browser_cfg.timeout);

    /* --- Memory (P7) --- */
    if (cfg->memory.char_limit < 100 || cfg->memory.char_limit > 1000000)
        add_issue(result, "memory.char_limit", "unreasonable %d", cfg->memory.char_limit);
    if (cfg->memory.ttl_days < 0 || cfg->memory.ttl_days > 36500)
        add_issue(result, "memory.ttl_days", "out of range 0-36500, got %d", cfg->memory.ttl_days);
    if (cfg->memory.storage_type < 0 || cfg->memory.storage_type > 3)
        add_issue(result, "memory.storage_type", "must be 0-3 (0=inmem,1=file,2=sqlite,3=plugin), got %d", cfg->memory.storage_type);

    /* --- Compression (P8) --- */
    if (cfg->compression.strategy[0] &&
        strcmp(cfg->compression.strategy, "smart") != 0 &&
        strcmp(cfg->compression.strategy, "summary") != 0 &&
        strcmp(cfg->compression.strategy, "extractive") != 0)
        add_issue(result, "compression.strategy", "unknown '%s' (smart/summary/extractive)", cfg->compression.strategy);
    if (cfg->compression.target_ratio < 0.1f || cfg->compression.target_ratio > 0.9f)
        add_issue(result, "compression.target_ratio", "must be 0.1-0.9, got %.2f", cfg->compression.target_ratio);
    if (cfg->compression.min_messages < 2 || cfg->compression.min_messages > 1000)
        add_issue(result, "compression.min_messages", "unreasonable %d", cfg->compression.min_messages);

    /* --- Cron (P9) --- */
    if (cfg->cron.max_concurrent_jobs < 0 || cfg->cron.max_concurrent_jobs > 100)
        add_issue(result, "cron.max_concurrent_jobs", "unreasonable %d", cfg->cron.max_concurrent_jobs);
    if (cfg->cron.job_timeout < 1 || cfg->cron.job_timeout > 86400)
        add_issue(result, "cron.job_timeout", "unreasonable %d", cfg->cron.job_timeout);
    if (cfg->cron.retention_days < 0 || cfg->cron.retention_days > 36500)
        add_issue(result, "cron.retention_days", "unreasonable %d", cfg->cron.retention_days);

    /* --- Plugin (P13) --- */
    if (cfg->plugin.dirs[0] == '\0')
        add_issue(result, "plugin.dirs", "plugin directories not configured");

    /* --- MCP (P14) --- */
    if (cfg->mcp.timeout < 1 || cfg->mcp.timeout > 300)
        add_issue(result, "mcp.timeout", "unreasonable %d", cfg->mcp.timeout);
    if (cfg->mcp.max_tools < 1 || cfg->mcp.max_tools > 256)
        add_issue(result, "mcp.max_tools", "unreasonable %d", cfg->mcp.max_tools);

    /* --- Terminal --- */
    if (cfg->terminal.timeout < 1 || cfg->terminal.timeout > 86400)
        add_issue(result, "terminal.timeout", "unreasonable %d", cfg->terminal.timeout);
    if (cfg->terminal.backend[0] &&
        strcmp(cfg->terminal.backend, "local") != 0 &&
        strcmp(cfg->terminal.backend, "ssh") != 0 &&
        strcmp(cfg->terminal.backend, "docker") != 0 &&
        strcmp(cfg->terminal.backend, "modal") != 0 &&
        strcmp(cfg->terminal.backend, "daytona") != 0 &&
        strcmp(cfg->terminal.backend, "singularity") != 0)
        add_issue(result, "terminal.backend", "unknown '%s' (local/ssh/docker/modal/daytona/singularity)", cfg->terminal.backend);
    if (cfg->terminal.container_cpu < 1 || cfg->terminal.container_cpu > 128)
        add_issue(result, "terminal.container_cpu", "unreasonable %d", cfg->terminal.container_cpu);
    if (cfg->terminal.container_memory < 128 || cfg->terminal.container_memory > 1048576)
        add_issue(result, "terminal.container_memory", "unreasonable %d MB", cfg->terminal.container_memory);

    /* --- Logging --- */
    if (cfg->logging.level[0] &&
        strcmp(cfg->logging.level, "debug") != 0 &&
        strcmp(cfg->logging.level, "info") != 0 &&
        strcmp(cfg->logging.level, "warning") != 0 &&
        strcmp(cfg->logging.level, "error") != 0)
        add_issue(result, "logging.level", "unknown '%s' (debug/info/warning/error)", cfg->logging.level);
    if (cfg->logging.format[0] &&
        strcmp(cfg->logging.format, "text") != 0 &&
        strcmp(cfg->logging.format, "json") != 0)
        add_issue(result, "logging.format", "unknown '%s' (text/json)", cfg->logging.format);
    if (cfg->logging.max_files < 1 || cfg->logging.max_files > 1000)
        add_issue(result, "logging.max_files", "unreasonable %d", cfg->logging.max_files);
    if (cfg->logging.max_size_mb < 1 || cfg->logging.max_size_mb > 10240)
        add_issue(result, "logging.max_size_mb", "unreasonable %d MB", cfg->logging.max_size_mb);

    /* --- Skills --- */
    if (cfg->skills.validate_on_load < 0 || cfg->skills.validate_on_load > 2)
        add_issue(result, "skills.validate", "must be 0-2 (0=no,1=warn,2=strict), got %d", cfg->skills.validate_on_load);
    if (cfg->skills.bundle_size_limit < 1 || cfg->skills.bundle_size_limit > 65536)
        add_issue(result, "skills.bundle_size_limit", "unreasonable %d KB", cfg->skills.bundle_size_limit);

    /* --- Checkpoints --- */
    if (cfg->checkpoints.interval < 1 || cfg->checkpoints.interval > 1000)
        add_issue(result, "checkpoints.interval", "unreasonable %d turns", cfg->checkpoints.interval);
    if (cfg->checkpoints.max_checkpoints < 1 || cfg->checkpoints.max_checkpoints > 1000)
        add_issue(result, "checkpoints.max", "unreasonable %d", cfg->checkpoints.max_checkpoints);
    if (cfg->checkpoints.compression_level < 0 || cfg->checkpoints.compression_level > 9)
        add_issue(result, "checkpoints.compression", "must be 0-9, got %d", cfg->checkpoints.compression_level);

    /* --- Secrets --- */
    if (cfg->secrets.enabled && !cfg->secrets.access_token[0])
        add_issue(result, "secrets.bitwarden.access_token", "required when enabled=true");

    /* --- Auxiliary --- */
    if (cfg->auxiliary.vision.timeout < 1 || cfg->auxiliary.vision.timeout > 3600)
        add_issue(result, "auxiliary.vision.timeout", "unreasonable %d", cfg->auxiliary.vision.timeout);
    if (cfg->auxiliary.vision_download_timeout < 1 || cfg->auxiliary.vision_download_timeout > 300)
        add_issue(result, "auxiliary.vision.download_timeout", "unreasonable %d", cfg->auxiliary.vision_download_timeout);
    if (cfg->auxiliary.web_extract.timeout < 1 || cfg->auxiliary.web_extract.timeout > 3600)
        add_issue(result, "auxiliary.web_extract.timeout", "unreasonable %d", cfg->auxiliary.web_extract.timeout);

    /* --- TTS --- */
    if (cfg->tts.xai_sample_rate < 8000 || cfg->tts.xai_sample_rate > 192000)
        add_issue(result, "tts.xai.sample_rate", "unreasonable %d", cfg->tts.xai_sample_rate);

    /* --- STT --- */
    if (cfg->voice.silence_threshold < 0 || cfg->voice.silence_threshold > 32767)
        add_issue(result, "voice.silence_threshold", "unreasonable %d", cfg->voice.silence_threshold);

    return result ? result->count == 0 : true;
}
