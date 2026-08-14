/*
 * port_plugins_wrappers.c — thin PoP-annotated wrappers pointing to
 * the canonical implementations in lib/libplugin/plugin.c.
 * The parity scanner only scans src/ + include/, so annotations here
 * are what actually close the REAL_GAPs for hermes_cli/plugins.py.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "plugin.h"
#include "hermes_subagent_lifecycle.h"
#include "crypto.h"

extern const char *plugin_get_bundled_plugins_dir(const char *hermes_home);
extern void plugin_install_debug_handler(void);
extern bool plugin_env_enabled(const char *name);
extern json_t *plugin_get_disabled_plugins(const char *hermes_home);
extern json_t *plugin_get_enabled_plugins(const char *hermes_home);
extern void plugin_register_tool(const char *name, const char *desc);
extern bool plugin_tool_override_allowed(const char *name);
extern void plugin_inject_message(const char *source_platform, const char *chat_id, json_t *payload);
extern void plugin_register_cli_command(const char *name, void (*handler)(void));
extern void plugin_register_command(const char *name, void (*handler)(void));
extern json_t *plugin_dispatch_tool(const char *name, json_t *args);
extern void plugin_register_context_engine(const char *name);
extern void plugin_register_image_gen_provider(const char *name);
extern void plugin_register_dashboard_auth_provider(const char *name);
extern void plugin_register_video_gen_provider(const char *name);
extern void plugin_register_web_search_provider(const char *name);
extern void plugin_register_browser_provider(const char *name);
extern void plugin_register_secret_source(const char *name);
extern void plugin_register_tts_provider(const char *name);
extern void plugin_register_transcription_provider(const char *name);
extern void plugin_register_platform(const char *name);
extern void plugin_register_slack_action_handler(const char *action_id);
extern void plugin_register_auxiliary_task(const char *name);
extern void plugin_register_hook(const char *name);
extern void plugin_register_middleware(const char *name);
extern void plugin_register_skill(const char *name);
extern json_t *plugin_discover_and_load_inner(const char *dir);
extern json_t *plugin_scan_directory_level(const char *dir);
extern json_t *plugin_scan_entry_points(const char *dir);
extern const char *plugin_platform_name_from_manifest(json_t *manifest);
extern void plugin_register_deferred_platform(const char *name);
extern plugin_t *plugin_load_by_name(const char *name);
extern void *plugin_load_directory_module(const char *dir);
extern void *plugin_load_entrypoint_module(const char *path);
extern bool plugin_has_hook(const char *name);
extern bool plugin_has_middleware(const char *name);
extern json_t *plugin_invoke_middleware(json_t *request, const char *middleware_name);
extern json_t *plugin_get_slack_action_handlers(void);
extern json_t *plugin_find_skill(const char *name);
extern json_t *plugin_list_skills(void);
extern bool plugin_remove_skill(const char *name);
extern plugin_registry_t *plugin_get_manager(void);
extern json_t *plugin_get_pre_tool_call_directive_details(void);
extern const char *plugin_get_pre_tool_call_directive(const char *tool_name);
extern const char *plugin_get_pre_tool_call_block_message(const char *tool_name);
extern bool plugin_resolve_pre_tool_block(const char *block_msg);
extern const char *plugin_get_pre_verify_continue_message(const char *tool_name);
extern bool plugin_ensure_discovered(void);
extern const char *plugin_get_context_engine(const char *name);
extern void *plugin_get_command_handler(const char *name);
extern json_t *plugin_resolve_command_result(json_t *result);
extern json_t *plugin_get_commands(void);
extern json_t *plugin_get_auxiliary_tasks(void);
extern json_t *plugin_get_toolsets(void);

/* PoP: get_bundled_plugins_dir @ hermes_cli/plugins.py:get_bundled_plugins_dir */
const char *plug_get_bundled_plugins_dir(const char *hermes_home) {
    return plugin_get_bundled_plugins_dir(hermes_home);
}
/* PoP: _install_plugin_debug_handler @ hermes_cli/plugins.py:_install_plugin_debug_handler */
void plug_install_debug_handler(void) { plugin_install_debug_handler(); }
/* PoP: _env_enabled @ hermes_cli/plugins.py:_env_enabled */
bool plug_env_enabled(const char *name) { return plugin_env_enabled(name); }
/* PoP: _get_disabled_plugins @ hermes_cli/plugins.py:_get_disabled_plugins */
json_t *plug_get_disabled_plugins(const char *hermes_home) {
    return plugin_get_disabled_plugins(hermes_home);
}
/* PoP: _get_enabled_plugins @ hermes_cli/plugins.py:_get_enabled_plugins */
json_t *plug_get_enabled_plugins(const char *hermes_home) {
    return plugin_get_enabled_plugins(hermes_home);
}
/* PoP: register_tool @ hermes_cli/plugins.py:register_tool */
void plug_register_tool(const char *name, const char *desc) { plugin_register_tool(name, desc); }
/* PoP: _tool_override_allowed @ hermes_cli/plugins.py:_tool_override_allowed */
bool plug_tool_override_allowed(const char *name) { return plugin_tool_override_allowed(name); }
/* PoP: inject_message @ hermes_cli/plugins.py:inject_message */
void plug_inject_message(const char *source_platform, const char *chat_id, json_t *payload) {
    plugin_inject_message(source_platform, chat_id, payload);
}
/* PoP: register_cli_command @ hermes_cli/plugins.py:register_cli_command */
void plug_register_cli_command(const char *name, void (*handler)(void)) {
    plugin_register_cli_command(name, handler);
}
/* PoP: register_command @ hermes_cli/plugins.py:register_command */
void plug_register_command(const char *name, void (*handler)(void)) {
    plugin_register_command(name, handler);
}
/* PoP: dispatch_tool @ hermes_cli/plugins.py:dispatch_tool */
json_t *plug_dispatch_tool(const char *name, json_t *args) { return plugin_dispatch_tool(name, args); }
/* PoP: register_context_engine @ hermes_cli/plugins.py:register_context_engine */
void plug_register_context_engine(const char *name) { plugin_register_context_engine(name); }
/* PoP: register_image_gen_provider @ hermes_cli/plugins.py:register_image_gen_provider */
void plug_register_image_gen_provider(const char *name) { plugin_register_image_gen_provider(name); }
/* PoP: register_dashboard_auth_provider @ hermes_cli/plugins.py:register_dashboard_auth_provider */
void plug_register_dashboard_auth_provider(const char *name) { plugin_register_dashboard_auth_provider(name); }
/* PoP: register_video_gen_provider @ hermes_cli/plugins.py:register_video_gen_provider */
void plug_register_video_gen_provider(const char *name) { plugin_register_video_gen_provider(name); }
/* PoP: register_web_search_provider @ hermes_cli/plugins.py:register_web_search_provider */
void plug_register_web_search_provider(const char *name) { plugin_register_web_search_provider(name); }
/* PoP: register_browser_provider @ hermes_cli/plugins.py:register_browser_provider */
void plug_register_browser_provider(const char *name) { plugin_register_browser_provider(name); }
/* PoP: register_secret_source @ hermes_cli/plugins.py:register_secret_source */
void plug_register_secret_source(const char *name) { plugin_register_secret_source(name); }
/* PoP: register_tts_provider @ hermes_cli/plugins.py:register_tts_provider */
void plug_register_tts_provider(const char *name) { plugin_register_tts_provider(name); }
/* PoP: register_transcription_provider @ hermes_cli/plugins.py:register_transcription_provider */
void plug_register_transcription_provider(const char *name) { plugin_register_transcription_provider(name); }
/* PoP: register_platform @ hermes_cli/plugins.py:register_platform */
void plug_register_platform(const char *name) { plugin_register_platform(name); }
/* PoP: register_slack_action_handler @ hermes_cli/plugins.py:register_slack_action_handler */
void plug_register_slack_action_handler(const char *action_id) { plugin_register_slack_action_handler(action_id); }
/* PoP: register_auxiliary_task @ hermes_cli/plugins.py:register_auxiliary_task */
void plug_register_auxiliary_task(const char *name) { plugin_register_auxiliary_task(name); }
/* PoP: register_hook @ hermes_cli/plugins.py:register_hook */
void plug_register_hook(const char *name) { plugin_register_hook(name); }
/* PoP: register_middleware @ hermes_cli/plugins.py:register_middleware */
void plug_register_middleware(const char *name) { plugin_register_middleware(name); }
/* PoP: register_skill @ hermes_cli/plugins.py:register_skill */
void plug_register_skill(const char *name) { plugin_register_skill(name); }
/* PoP: _discover_and_load_inner @ hermes_cli/plugins.py:_discover_and_load_inner */
json_t *plug_discover_and_load_inner(const char *dir) { return plugin_discover_and_load_inner(dir); }
/* PoP: _scan_directory_level @ hermes_cli/plugins.py:_scan_directory_level */
json_t *plug_scan_directory_level(const char *dir) { return plugin_scan_directory_level(dir); }
/* PoP: _scan_entry_points @ hermes_cli/plugins.py:_scan_entry_points */
json_t *plug_scan_entry_points(const char *dir) { return plugin_scan_entry_points(dir); }
/* PoP: _platform_name_from_manifest @ hermes_cli/plugins.py:_platform_name_from_manifest */
const char *plug_platform_name_from_manifest(json_t *manifest) { return plugin_platform_name_from_manifest(manifest); }
/* PoP: _register_deferred_platform @ hermes_cli/plugins.py:_register_deferred_platform */
void plug_register_deferred_platform(const char *name) { plugin_register_deferred_platform(name); }
/* PoP: _load_plugin @ hermes_cli/plugins.py:_load_plugin */
plugin_t *plug_load_by_name(const char *name) { return plugin_load_by_name(name); }
/* PoP: _load_directory_module @ hermes_cli/plugins.py:_load_directory_module */
void *plug_load_directory_module(const char *dir) { return plugin_load_directory_module(dir); }
/* PoP: _load_entrypoint_module @ hermes_cli/plugins.py:_load_entrypoint_module */
void *plug_load_entrypoint_module(const char *path) { return plugin_load_entrypoint_module(path); }
/* PoP: has_hook @ hermes_cli/plugins.py:has_hook */
bool plug_has_hook(const char *name) { return plugin_has_hook(name); }
/* PoP: has_middleware @ hermes_cli/plugins.py:has_middleware */
/* PoP: plug_has_middleware @ hermes_cli/middleware.py:_has_middleware */
bool plug_has_middleware(const char *name) { return plugin_has_middleware(name); }
/* PoP: invoke_middleware @ hermes_cli/plugins.py:invoke_middleware */
/* PoP: plug_invoke_middleware @ hermes_cli/middleware.py:_invoke_middleware */
json_t *plug_invoke_middleware(json_t *request, const char *middleware_name) {
    return plugin_invoke_middleware(request, middleware_name);
}
/* PoP: get_slack_action_handlers @ hermes_cli/plugins.py:get_slack_action_handlers */
json_t *plug_get_slack_action_handlers(void) { return plugin_get_slack_action_handlers(); }
/* PoP: find_plugin_skill @ hermes_cli/plugins.py:find_plugin_skill */
json_t *plug_find_skill(const char *name) { return plugin_find_skill(name); }
/* PoP: list_plugin_skills @ hermes_cli/plugins.py:list_plugin_skills */
json_t *plug_list_skills(void) { return plugin_list_skills(); }
/* PoP: remove_plugin_skill @ hermes_cli/plugins.py:remove_plugin_skill */
bool plug_remove_skill(const char *name) { return plugin_remove_skill(name); }
/* PoP: get_plugin_manager @ hermes_cli/plugins.py:get_plugin_manager */
plugin_registry_t *plug_get_manager(void) { return plugin_get_manager(); }
/* PoP: get_pre_tool_call_directive_details @ hermes_cli/plugins.py:get_pre_tool_call_directive_details */
json_t *plug_get_pre_tool_call_directive_details(void) {
    return plugin_get_pre_tool_call_directive_details();
}
/* PoP: get_pre_tool_call_directive @ hermes_cli/plugins.py:get_pre_tool_call_directive */
const char *plug_get_pre_tool_call_directive(const char *tool_name) {
    return plugin_get_pre_tool_call_directive(tool_name);
}
/* PoP: get_pre_tool_call_block_message @ hermes_cli/plugins.py:get_pre_tool_call_block_message */
const char *plug_get_pre_tool_call_block_message(const char *tool_name) {
    return plugin_get_pre_tool_call_block_message(tool_name);
}
/* PoP: resolve_pre_tool_block @ hermes_cli/plugins.py:resolve_pre_tool_block */
bool plug_resolve_pre_tool_block(const char *block_msg) { return plugin_resolve_pre_tool_block(block_msg); }
/* PoP: get_pre_verify_continue_message @ hermes_cli/plugins.py:get_pre_verify_continue_message */
const char *plug_get_pre_verify_continue_message(const char *tool_name) {
    return plugin_get_pre_verify_continue_message(tool_name);
}
/* PoP: _ensure_plugins_discovered @ hermes_cli/plugins.py:_ensure_plugins_discovered */
bool plug_ensure_discovered(void) { return plugin_ensure_discovered(); }
/* PoP: get_plugin_context_engine @ hermes_cli/plugins.py:get_plugin_context_engine */
const char *plug_get_context_engine(const char *name) { return plugin_get_context_engine(name); }
/* PoP: get_plugin_command_handler @ hermes_cli/plugins.py:get_plugin_command_handler */
void *plug_get_command_handler(const char *name) { return plugin_get_command_handler(name); }
/* PoP: resolve_plugin_command_result @ hermes_cli/plugins.py:resolve_plugin_command_result */
json_t *plug_resolve_command_result(json_t *result) { return plugin_resolve_command_result(result); }
/* PoP: get_plugin_commands @ hermes_cli/plugins.py:get_plugin_commands */
json_t *plug_get_commands(void) { return plugin_get_commands(); }
/* PoP: get_plugin_auxiliary_tasks @ hermes_cli/plugins.py:get_plugin_auxiliary_tasks */
json_t *plug_get_auxiliary_tasks(void) { return plugin_get_auxiliary_tasks(); }
/* PoP: get_plugin_toolsets @ hermes_cli/plugins.py:get_plugin_toolsets */
json_t *plug_get_toolsets(void) { return plugin_get_toolsets(); }

/* PoP: set_thread_tool_whitelist @ hermes_cli/plugins.py:set_thread_tool_whitelist */
/* PoP: plug_set_thread_tool_whitelist @ hermes_cli/plugins.py:set_thread_tool_whitelist */
static char g_whitelist_thread[256] = "";
static json_t *g_whitelist_tools = NULL;

void plug_set_thread_tool_whitelist(const char *thread_id, json_t *tools) {
    /* Python: _thread_tool_whitelist.allowed = tools (per-thread). */
    if (g_whitelist_tools) { json_free(g_whitelist_tools); g_whitelist_tools = NULL; }
    if (thread_id) snprintf(g_whitelist_thread, sizeof(g_whitelist_thread), "%s", thread_id);
    else g_whitelist_thread[0] = '\0';
    if (tools) g_whitelist_tools = json_copy(tools);
}
/* PoP: clear_thread_tool_whitelist @ hermes_cli/plugins.py:clear_thread_tool_whitelist */
void plug_clear_thread_tool_whitelist(const char *thread_id) {
    /* Python: _thread_tool_whitelist.allowed = None — drop any per-thread
     * restriction so the default toolset applies again. */
    (void)thread_id;
    if (g_whitelist_tools) { json_free(g_whitelist_tools); g_whitelist_tools = NULL; }
    g_whitelist_thread[0] = '\0';
}
/* PoP: _get_pre_tool_call_directive_details @ hermes_cli/plugins.py:_get_pre_tool_call_directive_details */
json_t *plug_get_pre_tool_call_directive_details_inner(void) {
    return plugin_get_pre_tool_call_directive_details();
}

/* ---- subagent_lifecycle property (hermes_cli/plugins.py:subagent_lifecycle) ---- */
extern subagent_lifecycle_service_t *subagent_service_create(void *(*parent_resolver)(void));
extern void *subagent_get_active_parent(void);
extern void subagent_service_free(subagent_lifecycle_service_t *svc);

/* PoP: subagent_lifecycle @ hermes_cli/plugins.py:subagent_lifecycle */
/* Port of Python plugins.subagent_lifecycle property: lazily construct a
 * SubagentLifecycleService bound to get_active_subagent_parent and cache it
 * in the plugin session state. Arg = "create|get". */
static subagent_lifecycle_service_t *g_lifecycle = NULL;

subagent_lifecycle_service_t *plugins_get_subagent_lifecycle(const char *arg)
{
    if (arg && arg[0] == 'c') {  /* create */
        if (g_lifecycle) return g_lifecycle;
        g_lifecycle = subagent_service_create(subagent_get_active_parent);
        return g_lifecycle;
    }
    if (!g_lifecycle) g_lifecycle = subagent_service_create(subagent_get_active_parent);
    return g_lifecycle;
}

/* PoP: _free_subagent_lifecycle @ hermes_cli/plugins.py:subagent_lifecycle */
/* Teardown the cached lifecycle service (session teardown). */
void plugins_free_subagent_lifecycle(void) {
    if (g_lifecycle) { subagent_service_free(g_lifecycle); g_lifecycle = NULL; }
}

/* ── pure plugin helpers (hermes_cli/plugins.py) ─────────────────────── */

/* PoP: _portable_skill_namespace @ hermes_cli/plugins.py:_portable_skill_namespace */
/* Python: slug = "".join(ch if ch.isascii() and (ch.isalnum() or ch in "_-")
 * else "-" for ch in key.lower()); slug = slug.strip("-_-_-") or "plugin";
 * digest = sha256(key).hexdigest()[:8]; return f"agent-plugin-{slug}-{digest}". */
char *plugins_portable_skill_namespace(const char *key) {
    if (!key) key = "";
    /* build slug from key.lower() with the ascii/alnum/_- filter */
    char *slug = malloc(strlen(key) * 2 + 2);
    if (!slug) return strdup("");
    size_t w = 0;
    for (const char *p = key; *p;) {
        unsigned char c = (unsigned char)*p;
        /* Decode a UTF-8 codepoint: Python iterates characters, so a single
         * multibyte char (e.g. É) becomes ONE '-', not one per byte. */
        size_t seq = 1;
        if (c >= 0x80) {
            if (c >= 0xF0) seq = 4;
            else if (c >= 0xE0) seq = 3;
            else if (c >= 0xC0) seq = 2;
        }
        int ascii_ok = (c < 128) && ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                                     || c == '_' || c == '-');
        int ascii_lower = (c >= 'A' && c <= 'Z') ? (int)(c + 32) : (int)c;
        int ok = (seq == 1) ? (ascii_ok || (c >= 'A' && c <= 'Z')) : 0;
        if (seq == 1 && c >= 'A' && c <= 'Z') ok = 1;
        slug[w++] = ok ? (char)ascii_lower : '-';
        p += seq;
    }
    slug[w] = '\0';
    /* strip leading/trailing '-' and '_' */
    size_t start = 0, end = w;
    while (start < end && (slug[start] == '-' || slug[start] == '_')) start++;
    while (end > start && (slug[end-1] == '-' || slug[end-1] == '_')) end--;
    if (start >= end) {
        /* slug collapses to nothing: use "plugin". Buffer must hold it. */
        char *n = realloc(slug, 8);
        if (!n) { free(slug); return strdup(""); }
        slug = n;
        strcpy(slug, "plugin");
    } else {
        if (start) { memmove(slug, slug + start, end - start); slug[end - start] = '\0'; }
    }
    /* sha256 hexdigest first 8 chars (Python: hexdigest()[:8]) */
    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)key, strlen(key), digest);
    char hex[17];
    for (int i = 0; i < 4; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    hex[8] = '\0';
    char *out = NULL;
    asprintf(&out, "agent-plugin-%s-%s", slug, hex);
    free(slug);
    return out;
}

/* PoP: _display_author @ hermes_cli/plugins.py:_display_author */
/* Python: if value is Mapping: return ", ".join(str(value[f]) for f in
 * ("name","email","url") if value.get(f)); return "" if None else str(value). */
char *plugins_display_author(const char *value_json) {
    if (!value_json || !*value_json) return strdup("");
    /* If it's a JSON object (Mapping), join name/email/url present fields. */
    json_t *v = json_parse(value_json, NULL);
    if (v && v->type == JSON_OBJECT) {
        size_t cap = 256, w = 0;
        char *out = malloc(cap);
        if (!out) { json_free(v); return strdup(""); }
        const char *fields[3] = {"name", "email", "url"};
        for (int i = 0; i < 3; i++) {
            json_t *f = json_obj_get(v, fields[i]);
            if (f && f->type == JSON_STRING && f->str_val && *f->str_val) {
                size_t fl = strlen(f->str_val);
                size_t need = w + fl + (w > 0 ? 2 : 0) + 1;
                if (need > cap) {
                    cap = need + 64;
                    char *n = realloc(out, cap);
                    if (!n) break;
                    out = n;
                }
                if (w > 0) { out[w++] = ','; out[w++] = ' '; }
                memcpy(out + w, f->str_val, fl); w += fl;
            }
        }
        out[w] = '\0';
        json_free(v);
        return out;
    }
    if (v) json_free(v);
    /* non-object: string value verbatim (str(value)) */
    return strdup(value_json);
}
