/*
 * config_env.c -- extracted from cli/config.c monolith.
 * Real implementation of one config-lifecycle concern; public
 * hermes_config_* protos stay in include/hermes_core_types.h.
 */

#include "hermes.h"
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

bool hermes_config_load_env(hermes_config_t *cfg) {
    /* Environment overrides for all config fields */
    const char *v;

    v = getenv("HERMES_MODEL");
    if (v) {
        snprintf(cfg->model, sizeof(cfg->model), "%s", v);
        snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", v);
    }

    v = getenv("HERMES_PROVIDER");
    if (v) {
        snprintf(cfg->provider, sizeof(cfg->provider), "%s", v);
        snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", v);
    }

    v = getenv("HERMES_BASE_URL");
    if (v) {
        snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", v);
        snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", v);
    }

    v = getenv("HERMES_API_KEY");
    if (v) {
        snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", v);
        snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", v);
    }

    v = getenv("HERMES_MAX_TURNS");
    if (v) { int t = atoi(v); if (t > 0) {
        cfg->max_turns = t;
        cfg->agent.max_iterations = t;
    }}

    v = getenv("HERMES_SKIN");
    if (v) {
        snprintf(cfg->skin_path, sizeof(cfg->skin_path), "%s", v);
        snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);
    }

    v = getenv("HERMES_PERSONALITY");
    if (v) {
        snprintf(cfg->personality, sizeof(cfg->personality), "%s", v);
        snprintf(cfg->display.personality, sizeof(cfg->display.personality), "%s", v);
    }

    v = getenv("HERMES_VERBOSE");
    if (v) { int t = atoi(v); if (t >= 0 && t <= 2) {
        cfg->verbose = t;
        cfg->agent.verbose_level = t;
    }}

    v = getenv("HERMES_YOLO");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->yolo_mode = true;

    v = getenv("HERMES_FAST");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0)) {
        cfg->fast_mode = true;
        cfg->agent.fast_mode = true;
    }

    v = getenv("HERMES_COMPRESS");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->compress_enabled = true;

    /* P1 env overrides */
    v = getenv("HERMES_API_MODE");
    if (v) snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "%s", v);

    v = getenv("HERMES_MAX_TOKENS");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.max_tokens = t; }

    v = getenv("HERMES_TEMPERATURE");
    if (v) { float f = (float)atof(v); if (f >= 0.0f && f <= 2.0f) cfg->provider_cfg.temperature = f; }

    v = getenv("HERMES_PRESENCE_PENALTY");
    if (v) { float f = (float)atof(v); if (f >= -2.0f && f <= 2.0f) cfg->provider_cfg.presence_penalty = f; }

    v = getenv("HERMES_FREQUENCY_PENALTY");
    if (v) { float f = (float)atof(v); if (f >= -2.0f && f <= 2.0f) cfg->provider_cfg.frequency_penalty = f; }

    v = getenv("HERMES_SEED");
    if (v) { int t = atoi(v); if (t >= 0) cfg->provider_cfg.seed = t; }

    v = getenv("HERMES_LOGPROBS");
    if (v) cfg->provider_cfg.logprobs = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);

    v = getenv("HERMES_TOP_LOGPROBS");
    if (v) { int t = atoi(v); if (t >= 0) cfg->provider_cfg.top_logprobs = t; }

    v = getenv("HERMES_USER");
    if (v) snprintf(cfg->provider_cfg.user, sizeof(cfg->provider_cfg.user), "%s", v);

    v = getenv("HERMES_TOP_P");
    if (v) { float f = (float)atof(v); if (f >= 0.0f && f <= 1.0f) cfg->provider_cfg.top_p = f; }

    v = getenv("HERMES_FALLBACK_MODEL");
    if (v) snprintf(cfg->provider_cfg.fallback_model, sizeof(cfg->provider_cfg.fallback_model), "%s", v);

    v = getenv("HERMES_SERVICE_TIER");
    if (v) snprintf(cfg->provider_cfg.service_tier, sizeof(cfg->provider_cfg.service_tier), "%s", v);

    v = getenv("HERMES_REASONING_EFFORT");
    if (v) snprintf(cfg->provider_cfg.reasoning_effort, sizeof(cfg->provider_cfg.reasoning_effort), "%s", v);

    v = getenv("HERMES_RESPONSE_FORMAT");
    if (v) snprintf(cfg->provider_cfg.response_format, sizeof(cfg->provider_cfg.response_format), "%s", v);

    v = getenv("HERMES_METADATA");
    if (v) snprintf(cfg->provider_cfg.metadata, sizeof(cfg->provider_cfg.metadata), "%s", v);

    v = getenv("HERMES_TOOL_CHOICE");
    if (v) snprintf(cfg->provider_cfg.tool_choice, sizeof(cfg->provider_cfg.tool_choice), "%s", v);

    v = getenv("HERMES_PARALLEL_TOOL_CALLS");
    if (v) cfg->provider_cfg.parallel_tool_calls = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    v = getenv("HERMES_MAX_TOOL_CALLS");
    if (v) { int t = atoi(v); if (t >= 0) cfg->provider_cfg.max_tool_calls = t; }

    v = getenv("HERMES_N");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.n = t; }

    /* B30: top_k + candidate_count env vars */
    v = getenv("HERMES_TOP_K");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.top_k = t; }
    v = getenv("HERMES_CANDIDATE_COUNT");
    if (v) { int t = atoi(v); if (t > 0) cfg->provider_cfg.candidate_count = t; }

    /* B23: json_mode env var */
    v = getenv("HERMES_JSON_MODE");
    if (v) cfg->provider_cfg.json_mode = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    /* B24: response_format_strict env var */
    v = getenv("HERMES_RESPONSE_FORMAT_STRICT");
    if (v) cfg->provider_cfg.response_format_strict = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    /* B29: safety_settings env var */
    v = getenv("HERMES_SAFETY_SETTINGS");
    if (v) snprintf(cfg->provider_cfg.safety_settings, sizeof(cfg->provider_cfg.safety_settings), "%s", v);

    v = getenv("HERMES_EXTRA_BODY");
    if (v) snprintf(cfg->provider_cfg.extra_body, sizeof(cfg->provider_cfg.extra_body), "%s", v);

    /* B37: Azure deployment_id env var */
    v = getenv("HERMES_AZURE_DEPLOYMENT_ID");
    if (v) snprintf(cfg->provider_cfg.azure_deployment_id, sizeof(cfg->provider_cfg.azure_deployment_id), "%s", v);
    v = getenv("HERMES_AZURE_API_VERSION");
    if (v) snprintf(cfg->provider_cfg.azure_api_version, sizeof(cfg->provider_cfg.azure_api_version), "%s", v);

    /* B43-B46: OpenRouter provider env var */
    v = getenv("HERMES_OPENROUTER_PROVIDER");
    if (v) snprintf(cfg->provider_cfg.openrouter_provider, sizeof(cfg->provider_cfg.openrouter_provider), "%s", v);

    /* B39-B41: Bedrock provider env vars */
    v = getenv("HERMES_BEDROCK_INFERENCE_PROFILE");
    if (v) snprintf(cfg->provider_cfg.bedrock_inference_profile, sizeof(cfg->provider_cfg.bedrock_inference_profile), "%s", v);
    v = getenv("HERMES_BEDROCK_GUARDRAIL_CONFIG");
    if (v) snprintf(cfg->provider_cfg.bedrock_guardrail_config, sizeof(cfg->provider_cfg.bedrock_guardrail_config), "%s", v);
    v = getenv("HERMES_BEDROCK_TRACE_ENABLED");
    if (v) cfg->provider_cfg.bedrock_trace_enabled = (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0) ? false : true;

    /* P2 env overrides (display) */
    v = getenv("HERMES_SKIN");
    if (v) {
        snprintf(cfg->skin_path, sizeof(cfg->skin_path), "%s", v);
        snprintf(cfg->display.skin, sizeof(cfg->display.skin), "%s", v);
    }

    v = getenv("HERMES_PERSONALITY");
    if (v) {
        snprintf(cfg->personality, sizeof(cfg->personality), "%s", v);
        snprintf(cfg->display.personality, sizeof(cfg->display.personality), "%s", v);
    }

    v = getenv("HERMES_BANNER");
    if (v) snprintf(cfg->display.banner, sizeof(cfg->display.banner), "%s", v);

    v = getenv("HERMES_SPINNER");
    if (v) snprintf(cfg->display.spinner_style, sizeof(cfg->display.spinner_style), "%s", v);

    v = getenv("HERMES_STREAM");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.stream = true;

    v = getenv("HERMES_SHOW_REASONING");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->display.show_reasoning = false;

    v = getenv("HERMES_INDICATOR");
    if (v) snprintf(cfg->display.indicator, sizeof(cfg->display.indicator), "%s", v);

    v = getenv("HERMES_COMPACT");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.compact = true;

    v = getenv("HERMES_DISPLAY_LANGUAGE");
    if (v) snprintf(cfg->display.language, sizeof(cfg->display.language), "%s", v);

    v = getenv("HERMES_SHOW_COST");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.show_cost = true;

    v = getenv("HERMES_TIMESTAMPS");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->display.timestamps = true;

    /* P3 env overrides (agent) */
    v = getenv("HERMES_MAX_TOOL_CALLS_ROUND");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.max_tool_calls_round = t; }

    v = getenv("HERMES_MAX_OUTPUT_TOKENS");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.max_output_tokens = t; }

    v = getenv("HERMES_SYSTEM_PROMPT");
    if (v) snprintf(cfg->agent.system_prompt, sizeof(cfg->agent.system_prompt), "%s", v);

    v = getenv("HERMES_PROFILE");
    if (v) snprintf(cfg->agent.profile, sizeof(cfg->agent.profile), "%s", v);

    v = getenv("HERMES_COMPRESS_THRESHOLD");
    if (v) { float f = (float)atof(v); if (f > 0.0f && f <= 1.0f) cfg->agent.compress_threshold = f; }

    v = getenv("HERMES_AGENT_REASONING_EFFORT");
    if (v) snprintf(cfg->agent.reasoning_effort, sizeof(cfg->agent.reasoning_effort), "%s", v);

    v = getenv("HERMES_API_MAX_RETRIES");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.api_max_retries = t; }

    v = getenv("HERMES_CLARIFY_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->agent.clarify_timeout = t; }

    /* P4 env overrides (tools) */
    v = getenv("HERMES_ALLOW_BACKGROUND");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->tools.allow_background = false;

    v = getenv("HERMES_APPROVAL_MODE");
    if (v) snprintf(cfg->tools.approval_mode, sizeof(cfg->tools.approval_mode), "%s", v);

    v = getenv("HERMES_APPROVAL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.approval_timeout = t; }

    v = getenv("HERMES_MAX_RESULT_SIZE");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.max_result_size = t; }

    v = getenv("HERMES_TERMINAL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.terminal_timeout = t; }

    v = getenv("HERMES_VISION_MODEL");
    if (v) {
        snprintf(cfg->tools.vision_model, sizeof(cfg->tools.vision_model), "%s", v);
        snprintf(cfg->auxiliary.vision.model, sizeof(cfg->auxiliary.vision.model), "%s", v);
    }

    v = getenv("HERMES_VISION_TIMEOUT");
    if (v) {
        int t = atoi(v); if (t > 0) cfg->tools.vision_timeout = t;
        if (t > 0) cfg->auxiliary.vision.timeout = t;
    }

    /* Auxiliary env overrides — HERMES_AUX_<TASK>_<FIELD> */
    #define LOAD_AUX_ENV_STR(task, field) do { \
        char en[128]; snprintf(en, sizeof(en), "HERMES_AUX_" #task "_" #field); \
        const char *e = getenv(en); if (e) snprintf(cfg->auxiliary.task.field, sizeof(cfg->auxiliary.task.field), "%s", e); \
    } while(0)
    #define LOAD_AUX_ENV_INT(task, field) do { \
        char en[128]; snprintf(en, sizeof(en), "HERMES_AUX_" #task "_" #field); \
        const char *e = getenv(en); if (e) { int tv = atoi(e); if (tv > 0) cfg->auxiliary.task.field = tv; } \
    } while(0)

    LOAD_AUX_ENV_STR(vision, provider);
    LOAD_AUX_ENV_STR(vision, base_url);
    LOAD_AUX_ENV_STR(vision, api_key);
    {
        char en[128]; snprintf(en, sizeof(en), "HERMES_AUX_VISION_DOWNLOAD_TIMEOUT");
        const char *e = getenv(en); if (e) { int tv = atoi(e); if (tv > 0) cfg->auxiliary.vision_download_timeout = tv; }
    }
    LOAD_AUX_ENV_STR(vision, extra_body);

    LOAD_AUX_ENV_STR(web_extract, provider);
    LOAD_AUX_ENV_STR(web_extract, model);
    LOAD_AUX_ENV_STR(web_extract, base_url);
    LOAD_AUX_ENV_STR(web_extract, api_key);
    LOAD_AUX_ENV_INT(web_extract, timeout);
    LOAD_AUX_ENV_STR(web_extract, extra_body);

    LOAD_AUX_ENV_STR(compression, provider);
    LOAD_AUX_ENV_STR(compression, model);
    LOAD_AUX_ENV_STR(compression, base_url);
    LOAD_AUX_ENV_STR(compression, api_key);
    LOAD_AUX_ENV_INT(compression, timeout);
    LOAD_AUX_ENV_STR(compression, extra_body);

    LOAD_AUX_ENV_STR(skills_hub, provider);
    LOAD_AUX_ENV_STR(skills_hub, model);
    LOAD_AUX_ENV_STR(skills_hub, base_url);
    LOAD_AUX_ENV_STR(skills_hub, api_key);
    LOAD_AUX_ENV_INT(skills_hub, timeout);
    LOAD_AUX_ENV_STR(skills_hub, extra_body);

    LOAD_AUX_ENV_STR(approval, provider);
    LOAD_AUX_ENV_STR(approval, model);
    LOAD_AUX_ENV_STR(approval, base_url);
    LOAD_AUX_ENV_STR(approval, api_key);
    LOAD_AUX_ENV_INT(approval, timeout);
    LOAD_AUX_ENV_STR(approval, extra_body);

    LOAD_AUX_ENV_STR(mcp, provider);
    LOAD_AUX_ENV_STR(mcp, model);
    LOAD_AUX_ENV_STR(mcp, base_url);
    LOAD_AUX_ENV_STR(mcp, api_key);
    LOAD_AUX_ENV_INT(mcp, timeout);
    LOAD_AUX_ENV_STR(mcp, extra_body);

    LOAD_AUX_ENV_STR(title_generation, provider);
    LOAD_AUX_ENV_STR(title_generation, model);
    LOAD_AUX_ENV_STR(title_generation, base_url);
    LOAD_AUX_ENV_STR(title_generation, api_key);
    LOAD_AUX_ENV_INT(title_generation, timeout);
    LOAD_AUX_ENV_STR(title_generation, extra_body);

    LOAD_AUX_ENV_STR(triage_specifier, provider);
    LOAD_AUX_ENV_STR(triage_specifier, model);
    LOAD_AUX_ENV_STR(triage_specifier, base_url);
    LOAD_AUX_ENV_STR(triage_specifier, api_key);
    LOAD_AUX_ENV_INT(triage_specifier, timeout);
    LOAD_AUX_ENV_STR(triage_specifier, extra_body);

    LOAD_AUX_ENV_STR(kanban_decomposer, provider);
    LOAD_AUX_ENV_STR(kanban_decomposer, model);
    LOAD_AUX_ENV_STR(kanban_decomposer, base_url);
    LOAD_AUX_ENV_STR(kanban_decomposer, api_key);
    LOAD_AUX_ENV_INT(kanban_decomposer, timeout);
    LOAD_AUX_ENV_STR(kanban_decomposer, extra_body);

    LOAD_AUX_ENV_STR(profile_describer, provider);
    LOAD_AUX_ENV_STR(profile_describer, model);
    LOAD_AUX_ENV_STR(profile_describer, base_url);
    LOAD_AUX_ENV_STR(profile_describer, api_key);
    LOAD_AUX_ENV_INT(profile_describer, timeout);
    LOAD_AUX_ENV_STR(profile_describer, extra_body);

    LOAD_AUX_ENV_STR(curator, provider);
    LOAD_AUX_ENV_STR(curator, model);
    LOAD_AUX_ENV_STR(curator, base_url);
    LOAD_AUX_ENV_STR(curator, api_key);
    LOAD_AUX_ENV_INT(curator, timeout);
    LOAD_AUX_ENV_STR(curator, extra_body);

    #undef LOAD_AUX_ENV_STR
    #undef LOAD_AUX_ENV_INT

    /* TTS env overrides */
    v = getenv("HERMES_TTS_PROVIDER");
    if (v) snprintf(cfg->tts.provider, sizeof(cfg->tts.provider), "%s", v);
    v = getenv("HERMES_TTS_EDGE_VOICE");
    if (v) snprintf(cfg->tts.edge_voice, sizeof(cfg->tts.edge_voice), "%s", v);
    v = getenv("HERMES_TTS_ELEVENLABS_VOICE_ID");
    if (v) snprintf(cfg->tts.elevenlabs_voice_id, sizeof(cfg->tts.elevenlabs_voice_id), "%s", v);
    v = getenv("HERMES_TTS_OPENAI_VOICE");
    if (v) snprintf(cfg->tts.openai_voice, sizeof(cfg->tts.openai_voice), "%s", v);
    v = getenv("HERMES_TTS_XAI_VOICE_ID");
    if (v) snprintf(cfg->tts.xai_voice_id, sizeof(cfg->tts.xai_voice_id), "%s", v);
    v = getenv("HERMES_TTS_PIPER_VOICE");
    if (v) snprintf(cfg->tts.piper_voice, sizeof(cfg->tts.piper_voice), "%s", v);

    /* STT env overrides */
    v = getenv("HERMES_STT_PROVIDER");
    if (v) snprintf(cfg->stt.provider, sizeof(cfg->stt.provider), "%s", v);
    v = getenv("HERMES_STT_LOCAL_MODEL");
    if (v) snprintf(cfg->stt.local_model, sizeof(cfg->stt.local_model), "%s", v);

    /* Voice env overrides */
    v = getenv("HERMES_VOICE_RECORD_KEY");
    if (v) snprintf(cfg->voice.record_key, sizeof(cfg->voice.record_key), "%s", v);
    v = getenv("HERMES_VOICE_AUTO_TTS");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0)) cfg->voice.auto_tts = true;

    v = getenv("HERMES_TERMINAL_BACKEND");
    if (v) snprintf(cfg->tools.terminal_backend, sizeof(cfg->tools.terminal_backend), "%s", v);

    v = getenv("HERMES_PERSISTENT_SHELL");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->tools.persistent_shell = false;
    else if (v)
        cfg->tools.persistent_shell = true;

    v = getenv("HERMES_WEB_BACKEND");
    if (v) snprintf(cfg->tools.web_backend, sizeof(cfg->tools.web_backend), "%s", v);

    v = getenv("HERMES_WEB_SEARCH_BACKEND");
    if (v) snprintf(cfg->tools.web_search_backend, sizeof(cfg->tools.web_search_backend), "%s", v);

    v = getenv("HERMES_WEB_EXTRACT_BACKEND");
    if (v) snprintf(cfg->tools.web_extract_backend, sizeof(cfg->tools.web_extract_backend), "%s", v);

    v = getenv("HERMES_WEB_SEARCH_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->tools.web_search_timeout = t; }

    /* P5-P14 env overrides */
    v = getenv("HERMES_DELEGATION_MAX_CONCURRENT");
    if (v) { int t = atoi(v); if (t > 0) cfg->delegation.max_concurrent_children = t; }

    v = getenv("HERMES_DELEGATION_SPAWN_DEPTH");
    if (v) { int t = atoi(v); if (t > 0) cfg->delegation.max_spawn_depth = t; }

    v = getenv("HERMES_DELEGATION_CHILD_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->delegation.child_timeout = t; }

    v = getenv("HERMES_DELEGATION_CHILD_MODEL");
    if (v) snprintf(cfg->delegation.child_model, sizeof(cfg->delegation.child_model), "%s", v);

    v = getenv("HERMES_DELEGATION_CHILD_PROVIDER");
    if (v) snprintf(cfg->delegation.child_provider, sizeof(cfg->delegation.child_provider), "%s", v);

    v = getenv("HERMES_BROWSER_HEADLESS");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->browser_cfg.headless = false;

    v = getenv("HERMES_BROWSER_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->browser_cfg.timeout = t; }

    v = getenv("HERMES_MEMORY_PROVIDER");
    if (v) snprintf(cfg->memory.provider, sizeof(cfg->memory.provider), "%s", v);

    v = getenv("HERMES_COMPRESSION_STRATEGY");
    if (v) snprintf(cfg->compression.strategy, sizeof(cfg->compression.strategy), "%s", v);

    v = getenv("HERMES_COMPRESSION_PROTECT_LAST_N");
    if (v) { int t = atoi(v); if (t > 0) cfg->compression.protect_last_n = t; }

    v = getenv("HERMES_COMPRESSION_PROTECT_FIRST_N");
    if (v) { int t = atoi(v); if (t >= 0) cfg->compression.protect_first_n = t; }

    v = getenv("HERMES_COMPRESSION_HARD_LIMIT");
    if (v) { int t = atoi(v); if (t > 0) cfg->compression.hygiene_hard_message_limit = t; }

    v = getenv("HERMES_CRON_DIR");
    if (v) snprintf(cfg->cron.dir, sizeof(cfg->cron.dir), "%s", v);

    v = getenv("HERMES_CRON_MAX_JOBS");
    if (v) { int t = atoi(v); if (t > 0) cfg->cron.max_concurrent_jobs = t; }

    v = getenv("HERMES_NOTIFY_PROVIDER");
    if (v) snprintf(cfg->notification.provider, sizeof(cfg->notification.provider), "%s", v);

    v = getenv("HERMES_TIRITH_PATH");
    if (v) snprintf(cfg->security.tirith_path, sizeof(cfg->security.tirith_path), "%s", v);

    v = getenv("HERMES_TIRITH_ENABLED");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->security.tirith_enabled = false;

    v = getenv("HERMES_SESSION_RETENTION_DAYS");
    if (v) { int t = atoi(v); if (t > 0) cfg->session.retention_days = t; }

    v = getenv("HERMES_MCP_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->mcp.timeout = t; }

    v = getenv("HERMES_MCP_AUTH");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->mcp.auth_enabled = true;

    /* Terminal env overrides */
    v = getenv("HERMES_TERMINAL_BACKEND");
    if (v) {
        snprintf(cfg->terminal.backend, sizeof(cfg->terminal.backend), "%s", v);
        snprintf(cfg->tools.terminal_backend, sizeof(cfg->tools.terminal_backend), "%s", v);
    }
    v = getenv("HERMES_TERMINAL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) {
        cfg->terminal.timeout = t;
        cfg->tools.terminal_timeout = t;
    }}
    v = getenv("HERMES_TERMINAL_PERSISTENT_SHELL");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0)) {
        cfg->terminal.persistent_shell = false;
        cfg->tools.persistent_shell = false;
    }
    v = getenv("HERMES_TERMINAL_CWD");
    if (v) snprintf(cfg->terminal.cwd, sizeof(cfg->terminal.cwd), "%s", v);
    v = getenv("HERMES_TERMINAL_DOCKER_IMAGE");
    if (v) snprintf(cfg->terminal.docker_image, sizeof(cfg->terminal.docker_image), "%s", v);
    v = getenv("HERMES_TERMINAL_CONTAINER_CPU");
    if (v) { int t = atoi(v); if (t > 0) cfg->terminal.container_cpu = t; }
    v = getenv("HERMES_TERMINAL_CONTAINER_MEMORY");
    if (v) { int t = atoi(v); if (t > 0) cfg->terminal.container_memory = t; }
    v = getenv("HERMES_TERMINAL_CONTAINER_DISK");
    if (v) { int t = atoi(v); if (t > 0) cfg->terminal.container_disk = t; }
    v = getenv("HERMES_TERMINAL_CONTAINER_PERSISTENT");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->terminal.container_persistent = false;

    /* Logging env overrides */
    v = getenv("HERMES_LOG_LEVEL");
    if (v) snprintf(cfg->logging.level, sizeof(cfg->logging.level), "%s", v);
    v = getenv("HERMES_LOG_FORMAT");
    if (v) snprintf(cfg->logging.format, sizeof(cfg->logging.format), "%s", v);
    v = getenv("HERMES_LOG_DIR");
    if (v) snprintf(cfg->logging.dir, sizeof(cfg->logging.dir), "%s", v);
    v = getenv("HERMES_LOG_MAX_FILES");
    if (v) { int t = atoi(v); if (t > 0) cfg->logging.max_files = t; }
    v = getenv("HERMES_LOG_MAX_SIZE_MB");
    if (v) { int t = atoi(v); if (t > 0) cfg->logging.max_size_mb = t; }

    /* Skills env overrides */
    v = getenv("HERMES_SKILLS_DIR");
    if (v) snprintf(cfg->skills.dir, sizeof(cfg->skills.dir), "%s", v);
    v = getenv("HERMES_SKILL_SEARCH_PATHS");
    if (v) snprintf(cfg->agent.skill_search_paths, sizeof(cfg->agent.skill_search_paths), "%s", v);
    v = getenv("HERMES_MODEL_METADATA");
    if (v) snprintf(cfg->agent.model_metadata_path, sizeof(cfg->agent.model_metadata_path), "%s", v);
    v = getenv("HERMES_SKILLS_ENABLED");
    if (v) snprintf(cfg->skills.enabled, sizeof(cfg->skills.enabled), "%s", v);
    v = getenv("HERMES_SKILLS_AUTO_DISCOVER");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->skills.auto_discover = false;

    /* Checkpoints env overrides */
    v = getenv("HERMES_CHECKPOINTS_ENABLED");
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->checkpoints.enabled = false;
    v = getenv("HERMES_CHECKPOINTS_INTERVAL");
    if (v) { int t = atoi(v); if (t > 0) cfg->checkpoints.interval = t; }
    v = getenv("HERMES_CHECKPOINTS_MAX");
    if (v) { int t = atoi(v); if (t > 0) cfg->checkpoints.max_checkpoints = t; }
    v = getenv("HERMES_CHECKPOINTS_DIR");
    if (v) snprintf(cfg->checkpoints.dir, sizeof(cfg->checkpoints.dir), "%s", v);

    /* Secrets env overrides (L01: Bitwarden Secrets Manager) */
    v = getenv("HERMES_SECRETS_BITWARDEN_ENABLED");
    if (v && (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0))
        cfg->secrets.enabled = true;
    if (v && (strcmp(v, "0") == 0 || strcasecmp(v, "false") == 0))
        cfg->secrets.enabled = false;
    v = getenv("HERMES_SECRETS_BITWARDEN_ACCESS_TOKEN");
    if (v) snprintf(cfg->secrets.access_token, sizeof(cfg->secrets.access_token), "%s", v);
    v = getenv("HERMES_SECRETS_BITWARDEN_ORGANIZATION_ID");
    if (v) snprintf(cfg->secrets.organization_id, sizeof(cfg->secrets.organization_id), "%s", v);
    v = getenv("HERMES_SECRETS_BITWARDEN_BWS_PATH");
    if (v) snprintf(cfg->secrets.bws_path, sizeof(cfg->secrets.bws_path), "%s", v);
    v = getenv("HERMES_SECRETS_BITWARDEN_INSTALL_TIMEOUT");
    if (v) { int t = atoi(v); if (t > 0) cfg->secrets.install_timeout = t; }

    return true;
}
