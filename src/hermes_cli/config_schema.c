/*
 * config_schema.c — extracted concern module from config.c.
 * Self-contained, opaque struct, minimal includes, C11.
 */

#include "config_schema.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

struct config_schema { int unused; };
config_schema_t *config_schema_init(void) { return calloc(1, sizeof(config_schema_t)); }
void config_schema_cleanup(config_schema_t *s) { free(s); }

/* Helper: create schema property definition */
json_t *schema_prop(const char *type, const char *desc, const char *default_val) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string(type));
    if (desc && desc[0]) json_set(prop, "description", json_string(desc));
    if (default_val && default_val[0]) json_set(prop, "default", json_string(default_val));
    return prop;
}

json_t *schema_prop_int(const char *desc, int def, int min, int max) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("integer"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_number((double)def));
    json_set(prop, "minimum", json_number((double)min));
    json_set(prop, "maximum", json_number((double)max));
    return prop;
}

json_t *schema_prop_num(const char *desc, double def, double min, double max) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("number"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_number(def));
    json_set(prop, "minimum", json_number(min));
    json_set(prop, "maximum", json_number(max));
    return prop;
}

json_t *schema_prop_bool(const char *desc, bool def) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("boolean"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_bool(def));
    return prop;
}

void schema_add_enum(json_t *prop, const char **values, int count) {
    json_t *arr = json_array();
    for (int i = 0; i < count; i++)
        json_append(arr, json_string(values[i]));
    json_set(prop, "enum", arr);
}

/* Helper: write a config value line */
void exp_str(FILE *f, const char *key, const char *val) {
    if (val && val[0]) fprintf(f, "%s: %s\n", key, val);
}

void exp_int(FILE *f, const char *key, int val) {
    fprintf(f, "%s: %d\n", key, val);
}

void exp_bool(FILE *f, const char *key, bool val) {
    fprintf(f, "%s: %s\n", key, val ? "true" : "false");
}

void exp_float(FILE *f, const char *key, float val) {
    fprintf(f, "%s: %.2f\n", key, (double)val);
}


/* --- hermes_config_schema (extracted from config.c) --- */

char *hermes_config_schema(void) {
    json_t *root = json_object();
    json_set(root, "$schema", json_string("https://json-schema.org/draft-07/schema#"));
    json_set(root, "title", json_string("Hermes C Config"));
    json_set(root, "description", json_string("Configuration schema for Hermes C CLI"));

    /* Type definitions */
    json_t *defs = json_object();
    json_t *properties = json_object();

    /* config_version */
    json_set(properties, "config_version",
             schema_prop_int("Config file format version", 1, 1, 999));

    /* --- model --- */
    {
        json_t *model = json_object();
        json_set(model, "type", json_string("object"));
        json_set(model, "description", json_string("Provider, model, and API connection settings"));
        json_t *mprops = json_object();
        json_set(mprops, "default", schema_prop("string", "Model name", ""));
        json_set(mprops, "provider", schema_prop("string", "Provider name", ""));
        json_set(mprops, "base_url", schema_prop("string", "API base URL", ""));
        json_set(mprops, "api_mode", schema_prop("string", "API mode", "chat_completions"));
        json_set(mprops, "fallback_model", schema_prop("string", "Fallback model", ""));
        json_set(mprops, "fallback_providers", schema_prop("string", "Comma-separated fallback providers", ""));
        json_set(mprops, "service_tier", schema_prop("string", "Service tier", "auto"));
        json_set(mprops, "reasoning_effort", schema_prop("string", "Reasoning effort", "medium"));
        json_set(mprops, "max_tokens", schema_prop_int("Max output tokens", 4096, 1, 1048576));
        json_set(mprops, "temperature", schema_prop_num("Sampling temperature", 1.0, 0.0, 2.0));
        json_set(mprops, "top_p", schema_prop_num("Nucleus sampling", 1.0, 0.0, 1.0));
        json_set(model, "properties", mprops);
        json_set(properties, "model", model);
    }

    /* --- display --- */
    {
        json_t *disp = json_object();
        json_set(disp, "type", json_string("object"));
        json_set(disp, "description", json_string("UI theme, skin, streaming, language"));
        json_t *dprops = json_object();
        json_set(dprops, "skin", schema_prop("string", "Display skin/theme", "default"));
        json_set(dprops, "banner", schema_prop("string", "Custom banner text", ""));
        json_set(dprops, "spinner", schema_prop("string", "Spinner style", ""));
        json_set(dprops, "indicator", schema_prop("string", "Busy indicator style", ""));
        json_set(dprops, "language", schema_prop("string", "UI language", "en"));
        json_set(dprops, "personality", schema_prop("string", "System prompt override", ""));
        json_set(dprops, "footer", schema_prop("string", "Custom footer text", ""));
        json_set(dprops, "streaming", schema_prop_bool("Token streaming", false));
        json_set(dprops, "show_reasoning", schema_prop_bool("Show reasoning content", true));
        json_set(dprops, "compact", schema_prop_bool("Compact mode", false));
        json_set(dprops, "show_cost", schema_prop_bool("Show token cost", false));
        json_set(dprops, "timestamps", schema_prop_bool("Show message timestamps", false));
        json_set(dprops, "statusbar", schema_prop_bool("Show status bar", true));
        json_set(disp, "properties", dprops);
        json_set(properties, "display", disp);
    }

    /* --- agent --- */
    {
        json_t *agent = json_object();
        json_set(agent, "type", json_string("object"));
        json_set(agent, "description", json_string("Iterations, verbosity, system prompt"));
        json_t *aprops = json_object();
        json_set(aprops, "max_turns", schema_prop_int("Max tool-calling turns", 90, 1, 10000));
        json_set(aprops, "max_tool_calls_round", schema_prop_int("Max tool calls per turn", 0, 0, 1000));
        json_set(aprops, "max_output_tokens", schema_prop_int("Max output tokens per response", 4096, 1, 1048576));
        json_set(aprops, "verbose", schema_prop_int("Verbosity level", 0, 0, 2));
        json_set(aprops, "api_max_retries", schema_prop_int("API call retries", 3, 0, 100));
        json_set(aprops, "clarify_timeout", schema_prop_int("Clarify timeout seconds", 300, 0, 3600));
        json_set(aprops, "compress_threshold", schema_prop_num("Auto-compress ratio", 0.38, 0.0, 1.0));
        json_set(aprops, "system_prompt", schema_prop("string", "Custom system prompt", ""));
        json_set(aprops, "profile", schema_prop("string", "Active profile name", ""));
        json_set(aprops, "reasoning_effort", schema_prop("string", "Reasoning effort", "medium"));
        json_set(aprops, "fast", schema_prop_bool("Fast mode", false));
        json_set(aprops, "quiet", schema_prop_bool("Quiet mode", false));
        json_set(agent, "properties", aprops);
        json_set(properties, "agent", agent);
    }

    /* --- tools --- */
    {
        json_t *tools = json_object();
        json_set(tools, "type", json_string("object"));
        json_set(tools, "description", json_string("Terminal, approvals, vision settings"));
        json_t *tprops = json_object();
        json_set(tprops, "allow_background", schema_prop_bool("Allow background processes", true));
        json_set(tprops, "local_process_cleanup", schema_prop_bool("Auto-cleanup on exit", true));
        json_set(tprops, "approval_mode", schema_prop("string", "Approval mode", "manual"));
        const char *approval_modes[] = {"off", "manual", "auto"};
        schema_add_enum(json_obj_get(tprops, "approval_mode"), approval_modes, 3);
        json_set(tprops, "approval_timeout", schema_prop_int("Approval timeout seconds", 600, 0, 86400));
        json_set(tprops, "max_result_size", schema_prop_int("Max tool result bytes", 50000, 256, 10485760));
        json_set(tprops, "terminal_timeout", schema_prop_int("Terminal timeout seconds", 1800, 1, 86400));
        json_set(tprops, "vision_timeout", schema_prop_int("Vision timeout seconds", 300, 1, 3600));
        json_set(tprops, "vision_model", schema_prop("string", "Vision model name", ""));
        json_set(tprops, "terminal_backend", schema_prop("string", "Terminal backend", "local"));
        json_set(tprops, "persistent_shell", schema_prop_bool("Persistent shell across commands", true));
        json_set(tprops, "web_backend", schema_prop("string", "Web search backend (shared fallback)", ""));
        json_set(tprops, "web_search_backend", schema_prop("string", "Web search backend override", ""));
        json_set(tprops, "web_extract_backend", schema_prop("string", "Web extract backend override", ""));
        json_set(tprops, "web_search_timeout", schema_prop_int("Web search timeout seconds", 30, 1, 300));
        json_set(tools, "properties", tprops);
        json_set(properties, "tools", tools);
    }

    /* --- delegation --- */
    {
        json_t *del = json_object();
        json_set(del, "type", json_string("object"));
        json_set(del, "description", json_string("Subagent spawning and child config"));
        json_t *dprops = json_object();
        json_set(dprops, "max_concurrent_children", schema_prop_int("Max parallel subagents", 3, 1, 50));
        json_set(dprops, "max_spawn_depth", schema_prop_int("Max nesting depth", 1, 0, 10));
        json_set(dprops, "child_timeout", schema_prop_int("Child timeout seconds", 600, 1, 36000));
        json_set(dprops, "child_max_turns", schema_prop_int("Child max iterations", 50, 1, 1000));
        json_set(dprops, "child_model", schema_prop("string", "Child model override", ""));
        json_set(dprops, "child_provider", schema_prop("string", "Child provider override", ""));
        json_set(del, "properties", dprops);
        json_set(properties, "delegation", del);
    }

    /* --- browser --- */
    {
        json_t *browser = json_object();
        json_set(browser, "type", json_string("object"));
        json_set(browser, "description", json_string("CDP browser engine settings"));
        json_t *bprops = json_object();
        json_set(bprops, "cdp_url", schema_prop("string", "CDP WebSocket URL", ""));
        json_set(bprops, "engine", schema_prop("string", "Browser engine", ""));
        json_set(bprops, "headless", schema_prop_bool("Headless mode", true));
        json_set(bprops, "javascript", schema_prop_bool("Enable JavaScript", true));
        json_set(bprops, "viewport_width", schema_prop_int("Viewport width", 1280, 320, 7680));
        json_set(bprops, "viewport_height", schema_prop_int("Viewport height", 720, 240, 4320));
        json_set(bprops, "command_timeout", schema_prop_int("Browser command timeout", 30, 1, 300));
        json_set(browser, "properties", bprops);
        json_set(properties, "browser", browser);
    }

    /* --- memory --- */
    {
        json_t *mem = json_object();
        json_set(mem, "type", json_string("object"));
        json_set(mem, "description", json_string("Memory provider, char limits, TTL"));
        json_t *mprops = json_object();
        json_set(mprops, "provider", schema_prop("string", "Memory provider backend", ""));
        json_set(mprops, "char_limit", schema_prop_int("Memory char limit", 2200, 100, 100000));
        json_set(mprops, "user_char_limit", schema_prop_int("User profile char limit", 1375, 100, 50000));
        json_set(mprops, "ttl_days", schema_prop_int("Memory TTL days", 30, 1, 3650));
        json_set(mprops, "auto_save", schema_prop_bool("Auto-save memory", true));
        json_set(mem, "properties", mprops);
        json_set(properties, "memory", mem);
    }

    /* --- compression --- */
    {
        json_t *comp = json_object();
        json_set(comp, "type", json_string("object"));
        json_set(comp, "description", json_string("Context compression strategy and thresholds"));
        json_t *cprops = json_object();
        json_set(cprops, "model", schema_prop("string", "Compression model override", ""));
        json_set(cprops, "strategy", schema_prop("string", "Compression strategy", "smart"));
        json_set(cprops, "target_ratio", schema_prop_num("Target compression ratio", 0.20, 0.01, 1.0));
        json_set(cprops, "min_messages", schema_prop_int("Minimum messages before compress", 20, 2, 1000));
        json_set(cprops, "protect_last_n", schema_prop_int("Recent messages to keep", 20, 1, 100));
        json_set(cprops, "protect_first_n", schema_prop_int("Head messages to preserve", 3, 0, 20));
        json_set(cprops, "hygiene_hard_message_limit", schema_prop_int("Force-compress threshold", 400, 50, 10000));
        json_set(cprops, "abort_on_summary_failure", schema_prop_bool("Abort on LLM error", false));
        json_set(cprops, "preserve_system", schema_prop_bool("Preserve system prompt", true));
        json_set(comp, "properties", cprops);
        json_set(properties, "compression", comp);
    }

    /* --- cron --- */
    {
        json_t *cron = json_object();
        json_set(cron, "type", json_string("object"));
        json_set(cron, "description", json_string("Scheduler directory, job limits"));
        json_t *cprops = json_object();
        json_set(cprops, "dir", schema_prop("string", "Cron jobs directory", ""));
        json_set(cprops, "max_concurrent_jobs", schema_prop_int("Max concurrent jobs", 5, 1, 100));
        json_set(cprops, "job_timeout", schema_prop_int("Job timeout seconds", 3600, 1, 86400));
        json_set(cprops, "retention_days", schema_prop_int("Job retention days", 30, 0, 3650));
        json_set(cprops, "notify_on_failure", schema_prop_bool("Notify on job failure", true));
        json_set(cron, "properties", cprops);
        json_set(properties, "cron", cron);
    }

    /* --- notification --- */
    {
        json_t *notif = json_object();
        json_set(notif, "type", json_string("object"));
        json_set(notif, "description", json_string("Completion/error notification settings"));
        json_t *nprops = json_object();
        json_set(nprops, "provider", schema_prop("string", "Notification provider", ""));
        json_set(nprops, "sound", schema_prop("string", "Notification sound", ""));
        json_set(nprops, "on_complete", schema_prop_bool("Notify on complete", true));
        json_set(nprops, "on_error", schema_prop_bool("Notify on error", true));
        json_set(nprops, "on_approval", schema_prop_bool("Notify on approval request", false));
        json_set(notif, "properties", nprops);
        json_set(properties, "notification", notif);
    }

    /* --- security --- */
    {
        json_t *sec = json_object();
        json_set(sec, "type", json_string("object"));
        json_set(sec, "description", json_string("Tirith, URL safety, redaction"));
        json_t *sprops = json_object();
        json_set(sprops, "tirith_path", schema_prop("string", "Tirith policy path", ""));
        json_set(sprops, "redact_patterns", schema_prop("string", "Redaction patterns", ""));
        json_set(sprops, "tirith_timeout", schema_prop_int("Tirith timeout seconds", 5, 0, 300));
        json_set(sprops, "tirith_enabled", schema_prop_bool("Tirith security enabled", true));
        json_set(sprops, "allow_private_urls", schema_prop_bool("Allow private URLs", false));
        json_set(sprops, "website_blocklist_enabled", schema_prop_bool("Website blocklist enabled", false));
        json_set(sec, "properties", sprops);
        json_set(properties, "security", sec);
    }

    /* --- sessions --- */
    {
        json_t *sess = json_object();
        json_set(sess, "type", json_string("object"));
        json_set(sess, "description", json_string("DB path, retention, auto-save"));
        json_t *sprops = json_object();
        json_set(sprops, "db_path", schema_prop("string", "Sessions DB path", ""));
        json_set(sprops, "retention_days", schema_prop_int("Session retention days", 90, 0, 3650));
        json_set(sprops, "auto_save_interval", schema_prop_int("Auto-save interval turns", 10, 1, 1000));
        json_set(sprops, "compress", schema_prop_bool("Compress sessions", false));
        json_set(sprops, "store_trajectories", schema_prop_bool("Store tool trajectories", false));
        json_set(sess, "properties", sprops);
        json_set(properties, "sessions", sess);
    }

    /* --- plugin --- */
    {
        json_t *plug = json_object();
        json_set(plug, "type", json_string("object"));
        json_set(plug, "description", json_string("Plugin directories and enabled plugins"));
        json_t *pprops = json_object();
        json_set(pprops, "dirs", schema_prop("string", "Plugin directories (comma-separated)", ""));
        json_set(pprops, "enabled", schema_prop("string", "Enabled plugins (comma-separated)", ""));
        json_set(plug, "properties", pprops);
        json_set(properties, "plugin", plug);
    }

    /* --- mcp --- */
    {
        json_t *mcp = json_object();
        json_set(mcp, "type", json_string("object"));
        json_set(mcp, "description", json_string("MCP server timeout, auth, tool limit"));
        json_t *mprops = json_object();
        json_set(mprops, "timeout", schema_prop_int("MCP server timeout seconds", 120, 1, 3600));
        json_set(mprops, "max_tools", schema_prop_int("Max MCP tools per server", 50, 1, 500));
        json_set(mprops, "auth_enabled", schema_prop_bool("MCP auth enabled", false));
        json_set(mcp, "properties", mprops);
        json_set(properties, "mcp", mcp);
    }

    /* --- auxiliary --- */
    #define S_AUX_TASK(task, nm, desc, to) do {         json_t *obj = json_object();         json_set(obj, "type", json_string("object"));         json_set(obj, "description", json_string("Auxiliary " desc " LLM routing"));         json_t *oprops = json_object();         json_set(oprops, "provider", schema_prop("string", "Provider for " desc, "auto"));         json_set(oprops, "model", schema_prop("string", "Model for " desc, ""));         json_set(oprops, "base_url", schema_prop("string", "Base URL for " desc, ""));         json_set(oprops, "api_key", schema_prop("string", "API key for " desc, ""));         json_set(oprops, "timeout", schema_prop_int("Timeout for " desc, to, 1, 3600));         json_set(oprops, "extra_body", schema_prop("string", "Extra request body fields", ""));         json_set(obj, "properties", oprops);         json_set(properties, "auxiliary." #task, obj);     } while(0)
    S_AUX_TASK(vision, "vision", "vision analysis", 120);
    S_AUX_TASK(web_extract, "web_extract", "web page extraction", 360);
    S_AUX_TASK(compression, "compression", "context compression", 120);
    S_AUX_TASK(skills_hub, "skills_hub", "skill hub", 30);
    S_AUX_TASK(approval, "approval", "approval review", 30);
    S_AUX_TASK(mcp, "mcp", "MCP routing", 30);
    S_AUX_TASK(title_generation, "title_generation", "title generation", 30);
    S_AUX_TASK(triage_specifier, "triage_specifier", "Kanban triage specification", 120);
    S_AUX_TASK(kanban_decomposer, "kanban_decomposer", "Kanban decomposition", 180);
    S_AUX_TASK(profile_describer, "profile_describer", "profile description", 60);
    S_AUX_TASK(curator, "curator", "skill-usage review", 600);
    #undef S_AUX_TASK

    /* --- tts --- */
    {
        json_t *tts = json_object();
        json_set(tts, "type", json_string("object"));
        json_set(tts, "description", json_string("Text-to-speech configuration"));
        json_t *tprops = json_object();
        json_set(tprops, "provider", schema_prop("string", "TTS provider", "edge"));
        json_set(tprops, "edge.voice", schema_prop("string", "Edge TTS voice name", "en-US-AriaNeural"));
        json_set(tprops, "elevenlabs.voice_id", schema_prop("string", "ElevenLabs voice ID", "pNInz6obpgDQGcFmaJgB"));
        json_set(tprops, "openai.model", schema_prop("string", "OpenAI TTS model", "gpt-4o-mini-tts"));
        json_set(tprops, "openai.voice", schema_prop("string", "OpenAI TTS voice", "alloy"));
        json_set(tprops, "xai.sample_rate", schema_prop_int("xAI sample rate", 24000, 8000, 192000));
        json_set(tprops, "piper.voice", schema_prop("string", "Piper TTS voice", "en_US-lessac-medium"));
        json_set(tts, "properties", tprops);
        json_set(properties, "tts", tts);
    }

    /* --- stt --- */
    {
        json_t *stt = json_object();
        json_set(stt, "type", json_string("object"));
        json_set(stt, "description", json_string("Speech-to-text configuration"));
        json_t *sprops = json_object();
        json_set(sprops, "enabled", schema_prop_bool("STT enabled", true));
        json_set(sprops, "provider", schema_prop("string", "STT provider", "local"));
        json_set(sprops, "local.model", schema_prop("string", "Local whisper model", "base"));
        json_set(sprops, "openai.model", schema_prop("string", "OpenAI whisper model", "whisper-1"));
        json_set(stt, "properties", sprops);
        json_set(properties, "stt", stt);
    }

    /* --- voice --- */
    {
        json_t *voice = json_object();
        json_set(voice, "type", json_string("object"));
        json_set(voice, "description", json_string("Voice input recording settings"));
        json_t *vprops = json_object();
        json_set(vprops, "record_key", schema_prop("string", "Record key binding", "ctrl+b"));
        json_set(vprops, "max_recording_seconds", schema_prop_int("Max recording length", 120, 1, 600));
        json_set(vprops, "auto_tts", schema_prop_bool("Auto-TTS on voice input", false));
        json_set(vprops, "beep_enabled", schema_prop_bool("Record start/stop beeps", true));
        json_set(voice, "properties", vprops);
        json_set(properties, "voice", voice);
    }

    json_set(root, "properties", properties);
    json_set(root, "definitions", defs);

    char *result = json_serialize_pretty(root, 2);
    json_free(root);
    return result;
}
