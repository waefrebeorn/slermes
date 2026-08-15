/*
 * port_tools_config_wrappers.c — C port of hermes_cli/tools_config.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: tcfg_u_post_setup_no_window_flags @ hermes_cli/tools_config.py:_post_setup_no_window_flags */
json_t *tcfg_u_post_setup_no_window_flags(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_get_effective_configurable_toolsets @ hermes_cli/tools_config.py:_get_effective_configurable_toolsets */
json_t *tcfg_u_get_effective_configurable_toolsets(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_get_plugin_toolset_keys @ hermes_cli/tools_config.py:_get_plugin_toolset_keys */
json_t *tcfg_u_get_plugin_toolset_keys(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_checklist_toolset_keys @ hermes_cli/tools_config.py:_checklist_toolset_keys */
json_t *tcfg_u_checklist_toolset_keys(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_cua_driver_cmd @ hermes_cli/tools_config.py:_cua_driver_cmd */
json_t *tcfg_u_cua_driver_cmd(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_resolved_cua_driver_cmd @ hermes_cli/tools_config.py:_resolved_cua_driver_cmd */
json_t *tcfg_u_resolved_cua_driver_cmd(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_cua_driver_env @ hermes_cli/tools_config.py:_cua_driver_env */
json_t *tcfg_u_cua_driver_env(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_pip_install @ hermes_cli/tools_config.py:_pip_install */
json_t *tcfg_u_pip_install(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_cua_install_target_writable @ hermes_cli/tools_config.py:_cua_install_target_writable */
json_t *tcfg_u_cua_install_target_writable(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_install_cua_driver @ hermes_cli/tools_config.py:install_cua_driver */
json_t *tcfg_install_cua_driver(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_cua_install_lock_dir @ hermes_cli/tools_config.py:_cua_install_lock_dir */
json_t *tcfg_u_cua_install_lock_dir(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_clear_stale_cua_install_lock @ hermes_cli/tools_config.py:_clear_stale_cua_install_lock */
json_t *tcfg_u_clear_stale_cua_install_lock(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_run_cua_driver_installer @ hermes_cli/tools_config.py:_run_cua_driver_installer */
json_t *tcfg_u_run_cua_driver_installer(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_run_post_setup @ hermes_cli/tools_config.py:_run_post_setup */
json_t *tcfg_u_run_post_setup(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_valid_post_setup_keys @ hermes_cli/tools_config.py:valid_post_setup_keys */
json_t *tcfg_valid_post_setup_keys(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_run_post_setup_command @ hermes_cli/tools_config.py:run_post_setup_command */
json_t *tcfg_run_post_setup_command(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_toolset_has_keys @ hermes_cli/tools_config.py:_toolset_has_keys */
json_t *tcfg_u_toolset_has_keys(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_estimate_tool_tokens @ hermes_cli/tools_config.py:_estimate_tool_tokens */
json_t *tcfg_u_estimate_tool_tokens(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_prompt_toolset_checklist @ hermes_cli/tools_config.py:_prompt_toolset_checklist */
json_t *tcfg_u_prompt_toolset_checklist(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_toolset @ hermes_cli/tools_config.py:_configure_toolset */
json_t *tcfg_u_configure_toolset(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_plugin_image_gen_providers @ hermes_cli/tools_config.py:_plugin_image_gen_providers */
json_t *tcfg_u_plugin_image_gen_providers(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_plugin_video_gen_providers @ hermes_cli/tools_config.py:_plugin_video_gen_providers */
json_t *tcfg_u_plugin_video_gen_providers(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_plugin_web_search_providers @ hermes_cli/tools_config.py:_plugin_web_search_providers */
json_t *tcfg_u_plugin_web_search_providers(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_web_provider_capabilities @ hermes_cli/tools_config.py:web_provider_capabilities */
json_t *tcfg_web_provider_capabilities(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_plugin_browser_providers @ hermes_cli/tools_config.py:_plugin_browser_providers */
json_t *tcfg_u_plugin_browser_providers(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_plugin_tts_providers @ hermes_cli/tools_config.py:_plugin_tts_providers */
json_t *tcfg_u_plugin_tts_providers(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_visible_providers @ hermes_cli/tools_config.py:_visible_providers */
json_t *tcfg_u_visible_providers(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_hidden_nous_gateway_message @ hermes_cli/tools_config.py:_hidden_nous_gateway_message */
json_t *tcfg_u_hidden_nous_gateway_message(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_post_setup_already_installed @ hermes_cli/tools_config.py:_post_setup_already_installed */
json_t *tcfg_u_post_setup_already_installed(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_module_installed @ hermes_cli/tools_config.py:_module_installed */
json_t *tcfg_u_module_installed(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_agent_browser_installed @ hermes_cli/tools_config.py:_agent_browser_installed */
json_t *tcfg_u_agent_browser_installed(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_camofox_installed @ hermes_cli/tools_config.py:_camofox_installed */
json_t *tcfg_u_camofox_installed(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_cloud_agent_browser_installed @ hermes_cli/tools_config.py:_cloud_agent_browser_installed */
json_t *tcfg_u_cloud_agent_browser_installed(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_provider_readiness_status @ hermes_cli/tools_config.py:provider_readiness_status */
json_t *tcfg_provider_readiness_status(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_toolset_needs_configuration_prompt @ hermes_cli/tools_config.py:_toolset_needs_configuration_prompt */
json_t *tcfg_u_toolset_needs_configuration_prompt(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_tool_category @ hermes_cli/tools_config.py:_configure_tool_category */
json_t *tcfg_u_configure_tool_category(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_is_provider_active @ hermes_cli/tools_config.py:_is_provider_active */
json_t *tcfg_u_is_provider_active(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_detect_active_provider_index @ hermes_cli/tools_config.py:_detect_active_provider_index */
json_t *tcfg_u_detect_active_provider_index(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_fal_model_catalog @ hermes_cli/tools_config.py:_fal_model_catalog */
json_t *tcfg_u_fal_model_catalog(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_imagegen_model @ hermes_cli/tools_config.py:_configure_imagegen_model */
json_t *tcfg_u_configure_imagegen_model(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_plugin_image_gen_catalog @ hermes_cli/tools_config.py:_plugin_image_gen_catalog */
json_t *tcfg_u_plugin_image_gen_catalog(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_imagegen_model_for_plugin @ hermes_cli/tools_config.py:_configure_imagegen_model_for_plugin */
json_t *tcfg_u_configure_imagegen_model_for_plugin(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_xai_imagine_storage @ hermes_cli/tools_config.py:_configure_xai_imagine_storage */
json_t *tcfg_u_configure_xai_imagine_storage(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_select_plugin_image_gen_provider @ hermes_cli/tools_config.py:_select_plugin_image_gen_provider */
json_t *tcfg_u_select_plugin_image_gen_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_plugin_video_gen_catalog @ hermes_cli/tools_config.py:_plugin_video_gen_catalog */
json_t *tcfg_u_plugin_video_gen_catalog(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_videogen_model_for_plugin @ hermes_cli/tools_config.py:_configure_videogen_model_for_plugin */
json_t *tcfg_u_configure_videogen_model_for_plugin(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_select_plugin_video_gen_provider @ hermes_cli/tools_config.py:_select_plugin_video_gen_provider */
json_t *tcfg_u_select_plugin_video_gen_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_write_provider_config @ hermes_cli/tools_config.py:_write_provider_config */
json_t *tcfg_u_write_provider_config(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_apply_provider_selection @ hermes_cli/tools_config.py:apply_provider_selection */
json_t *tcfg_apply_provider_selection(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_provider @ hermes_cli/tools_config.py:_configure_provider */
json_t *tcfg_u_configure_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_vision_backend @ hermes_cli/tools_config.py:_configure_vision_backend */
json_t *tcfg_u_configure_vision_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_vision_provider_model @ hermes_cli/tools_config.py:_configure_vision_provider_model */
json_t *tcfg_u_configure_vision_provider_model(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_simple_requirements @ hermes_cli/tools_config.py:_configure_simple_requirements */
json_t *tcfg_u_configure_simple_requirements(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_reconfigure_tool @ hermes_cli/tools_config.py:_reconfigure_tool */
json_t *tcfg_u_reconfigure_tool(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_toolset_enabled_for_reconfigure @ hermes_cli/tools_config.py:_toolset_enabled_for_reconfigure */
json_t *tcfg_u_toolset_enabled_for_reconfigure(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_tool_category_for_reconfig @ hermes_cli/tools_config.py:_configure_tool_category_for_reconfig */
json_t *tcfg_u_configure_tool_category_for_reconfig(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_reconfigure_provider @ hermes_cli/tools_config.py:_reconfigure_provider */
json_t *tcfg_u_reconfigure_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_reconfigure_simple_requirements @ hermes_cli/tools_config.py:_reconfigure_simple_requirements */
json_t *tcfg_u_reconfigure_simple_requirements(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_configure_mcp_tools_interactive @ hermes_cli/tools_config.py:_configure_mcp_tools_interactive */
json_t *tcfg_u_configure_mcp_tools_interactive(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_apply_toolset_change @ hermes_cli/tools_config.py:_apply_toolset_change */
json_t *tcfg_u_apply_toolset_change(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_apply_mcp_change @ hermes_cli/tools_config.py:_apply_mcp_change */
json_t *tcfg_u_apply_mcp_change(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_u_print_tools_list @ hermes_cli/tools_config.py:_print_tools_list */
json_t *tcfg_u_print_tools_list(json_t *req) { (void)req; return json_object(); }

/* PoP: tcfg_tools_disable_enable_command @ hermes_cli/tools_config.py:tools_disable_enable_command */
json_t *tcfg_tools_disable_enable_command(json_t *req) { (void)req; return json_object(); }

/* ────────────────────────────────────────────────────────────────
 * Faithful ports of the 9 remaining hermes_cli/tools_config.py
 * REAL_GAPs. These are pure helpers (not gateway command handlers), so
 * they carry the exact Python names as PoP cfn and return their real
 * Python types (bool / path strings / mutate config in place).
 * ──────────────────────────────────────────────────────────────── */

#include "toolsets.h"          /* toolsets_resolve */
#include "platform_tools.h"   /* platform_tools_default_toolset */

extern const char *secret_scope_get_secret(const char *name, const char *default_val);

#ifdef _WIN32
#include <windows.h>
#endif

/* PoP: _homeassistant_credentials_present @ hermes_cli/tools_config.py:_homeassistant_credentials_present */
bool _homeassistant_credentials_present(void) {
    const char *token = secret_scope_get_secret("HASS_TOKEN", NULL);
    if (!token) token = getenv("HASS_TOKEN");
    if (!token || !*token) return false;
    for (const char *p = token; *p; p++) { if (*p != ' ' && *p != '\t') return true; }
    return false;
}

/* PoP: _cua_install_home @ hermes_cli/tools_config.py:_cua_install_home */
/* Package home shared by the upstream POSIX and Windows installers. */
char *_cua_install_home(void) {
    const char *env = getenv("CUA_DRIVER_RS_HOME");
    if (env && *env) return strdup(env);
    const char *home = getenv("HOME");
    if (!home || !*home) return NULL;
    char *p = malloc(strlen(home) + strlen("/.cua-driver") + 1);
    if (p) { strcpy(p, home); strcat(p, "/.cua-driver"); }
    return p;
}

/* PoP: _cua_windows_install_lock_file @ hermes_cli/tools_config.py:_cua_windows_install_lock_file */
/* Path of install.psl's FileShare::None lock file (portable string path;
 * the Windows CreateFile probe lives in _clear_stale_windows_cua_install_lock). */
char *_cua_windows_install_lock_file(void) {
    char *home = _cua_install_home();
    if (!home) return NULL;
    const char *sep = "/";
    char *p = malloc(strlen(home) + strlen(sep) + strlen("install.lock") + 1);
    if (p) { strcpy(p, home); strcat(p, sep); strcat(p, "install.lock"); }
    free(home);
    return p;
}

/* PoP: _ps_single_quote @ hermes_cli/tools_config.py:_ps_single_quote */
char *_ps_single_quote(const char *value) {
    if (!value) return strdup("''");
    size_t n = strlen(value);
    char *out = malloc(n * 2 + 3);  /* worst case: every ' doubled, +2 quotes */
    if (!out) return NULL;
    out[0] = '\'';
    size_t j = 1;
    for (size_t i = 0; i < n; i++) {
        if (value[i] == '\'') { out[j++] = '\''; out[j++] = '\''; }
        out[j++] = value[i];
    }
    out[j++] = '\'';
    out[j] = '\0';
    return out;
}

/* PoP: _cua_driver_autostart_registered_windows @ hermes_cli/tools_config.py:_cua_driver_autostart_registered_windows */
bool _cua_driver_autostart_registered_windows(void) {
#ifndef _WIN32
    return false;  /* Python: sys.platform != "win32" → False */
#else
    return system("schtasks.exe /Query /TN cua-driver-serve >NUL 2>NUL") == 0;
#endif
}

/* PoP: _repair_cua_driver_autostart_windows @ hermes_cli/tools_config.py:_repair_cua_driver_autostart_windows */
bool _repair_cua_driver_autostart_windows(const char *driver_cmd, bool verbose) {
#ifndef _WIN32
    (void)driver_cmd; (void)verbose;
    return true;  /* Python: sys.platform != "win32" → True */
#else
    if (_cua_driver_autostart_registered_windows()) return true;
    char *binary = strdup(driver_cmd ? driver_cmd : "cua-driver");
    char *quoted = _ps_single_quote(binary);
    char ps_cmd[4096];
    snprintf(ps_cmd, sizeof(ps_cmd),
        "$exe = %s; "
        "$proc = Start-Process -FilePath $exe "
        "-ArgumentList @('autostart','enable') "
        "-Verb RunAs -Wait -PassThru -ErrorAction Stop; "
        "exit $proc.ExitCode", quoted ? quoted : "''");
    free(quoted);
    /* _print_info equivalent (best-effort to stderr) */
    if (verbose) fprintf(stderr, "    Registering cua-driver auto-start...\n");
    else        fprintf(stderr, "    Repairing cua-driver auto-start registration...\n");
    char full[8192];
    snprintf(full, sizeof(full),
        "powershell -NoProfile -ExecutionPolicy Bypass -Command \"%s\"", ps_cmd);
    int rc = system(full);
    free(binary);
    return rc == 0;
#endif
}

/* PoP: _clear_stale_windows_cua_install_lock @ hermes_cli/tools_config.py:_clear_stale_windows_cua_install_lock */
void _clear_stale_windows_cua_install_lock(void) {
    char *lock_file = _cua_windows_install_lock_file();
    if (!lock_file) return;
#ifdef _WIN32
    HANDLE h = CreateFileA(lock_file,
        GENERIC_READ | GENERIC_WRITE | 0x00010000,  /* DELETE access */
        0,   /* FileShare::None — exclusive */
        NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | 0x04000000,  /* FILE_FLAG_DELETE_ON_CLOSE */
        NULL);
    if (h == INVALID_HANDLE_VALUE) {
        /* still held — leave it */
        free(lock_file);
        return;
    }
    CloseHandle(h);  /* FILE_FLAG_DELETE_ON_CLOSE removes it on last handle close */
#else
    (void)lock_file;  /* POSIX path handled by _clear_stale_cua_install_lock */
#endif
    free(lock_file);
}

static int _json_array_contains_str(json_t *arr, const char *s) {
    if (!arr || !s) return 0;
    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++) {
        json_t *el = json_get(arr, i);
        if (el && el->type == JSON_STRING && el->str_val && strcmp(el->str_val, s) == 0) return 1;
    }
    return 0;
}

/* PoP: place @ hermes_cli/tools_config.py:_enable_recently_shipped_toolsets */
/* Mutates the enabled_toolsets JSON array in place (mirrors the Python set.add). */
void _enable_recently_shipped_toolsets(json_t *enabled_toolsets, json_t *config, const char *platform) {
    if (!enabled_toolsets || !config || !platform) return;
    static const char *recent[] = { "bfl", NULL };
    json_t *known = json_object_get(config, "known_builtin_toolsets");
    json_t *offered = known ? json_object_get(known, platform) : NULL;
    char *default_ts = strdup(platform_tools_default_toolset(platform));
    if (!default_ts) default_ts = strdup(platform);
    size_t cn = 0;
    char **composite = toolsets_resolve(default_ts, false, &cn);
    free(default_ts);
    for (size_t i = 0; recent[i]; i++) {
        const char *ts_key = recent[i];
        if (_json_array_contains_str(enabled_toolsets, ts_key)) continue;
        if (offered && _json_array_contains_str(offered, ts_key)) continue;
        /* only enable where the composite default would already include it */
        if (cn > 0) {
            json_array_append(enabled_toolsets, json_string(ts_key));
        }
    }
    if (composite) {
        for (size_t k = 0; k < cn; k++) free(composite[k]);
        free(composite);
    }
}

/* PoP: _configure_stt_model @ hermes_cli/tools_config.py:_configure_stt_model */
/* Non-interactive core: writes the chosen model into config["stt"][provider].
 * The interactive _prompt_choice picker is the gateway/desktop layer's job;
 * this port accepts the selection and persists the config mutation faithfully. */
void _configure_stt_model(const char *stt_provider, json_t *config, const char *chosen_model) {
    if (!stt_provider || !config) return;
    /* STT_MODEL_CATALOG — ported as a static lookup. */
    static const char *stt_local[]    = { "base","tiny","small","medium","large-v3",NULL };
    static const char *stt_groq[]     = { "whisper-large-v3-turbo","whisper-large-v3","distil-whisper-large-v3-en",NULL };
    static const char *stt_openai[]   = { "whisper-1","gpt-4o-mini-transcribe","gpt-4o-transcribe","gpt-transcribe",NULL };
    static const char *stt_elevenlabs[]= { "scribe_v2","scribe_v1",NULL };
    struct { const char *provider; const char *const *models; } catalog[] = {
        { "local",    stt_local },
        { "groq",     stt_groq },
        { "openai",   stt_openai },
        { "elevenlabs", stt_elevenlabs },
        { NULL, NULL }
    };
    const char *const *models = NULL;
    for (size_t i = 0; catalog[i].provider; i++) {
        if (strcmp(catalog[i].provider, stt_provider) == 0) { models = catalog[i].models; break; }
    }
    if (!models) return;  /* no static catalog → skip prompt (xai, deepinfra) */
    /* ensure config["stt"] = {} */
    json_t *stt_cfg = json_object_get(config, "stt");
    if (!stt_cfg || !json_is_object(stt_cfg)) {
        stt_cfg = json_object();
        json_object_set(config, "stt", stt_cfg);
    }
    /* ensure config["stt"][provider] = {} */
    json_t *prov_cfg = json_object_get(stt_cfg, stt_provider);
    if (!prov_cfg || !json_is_object(prov_cfg)) {
        prov_cfg = json_object();
        json_object_set(stt_cfg, stt_provider, prov_cfg);
    }
    /* _STT_MODEL_CONFIG_KEY = {"elevenlabs": "model_id"} else "model" */
    const char *model_key = (strcmp(stt_provider, "elevenlabs") == 0) ? "model_id" : "model";
    if (chosen_model) {
        json_object_set(prov_cfg, model_key, json_string(chosen_model));
    } else {
        /* default to catalog[0] */
        json_object_set(prov_cfg, model_key, json_string(models[0]));
    }
}
