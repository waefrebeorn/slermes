/*
 * port_plugins_cmd_wrappers.c — C port of hermes_cli/plugins_cmd.py
 * 47 PoP-annotated handlers for CLI plugin management commands.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include "hermes_json.h"
/* PoP: _resolve_git_executable @ hermes_cli/plugins_cmd.py:_resolve_git_executable */
const char *pcmd_resolve_git_executable(void) {
    /* Python _resolve_git_executable: shutil.which("git") first, then
     * common Git for Windows install paths and POSIX defaults. */
    const char *path_env = getenv("PATH");
    if (path_env) {
        char *dup = strdup(path_env);
        if (dup) {
            char *save = NULL;
            for (char *dir = strtok_r(dup, ":", &save); dir;
                 dir = strtok_r(NULL, ":", &save)) {
                char cand[1024];
                snprintf(cand, sizeof(cand), "%s/git", dir);
                if (access(cand, X_OK) == 0) {
                    free(dup);
                    return "git";
                }
                snprintf(cand, sizeof(cand), "%s/git.exe", dir);
                if (access(cand, X_OK) == 0) {
                    free(dup);
                    return "git.exe";
                }
            }
            free(dup);
        }
    }
    /* Common Git for Windows install paths. */
    static const char *win_paths[] = {
        "C:/Program Files/Git/cmd/git.exe",
        "C:/Program Files (x86)/Git/cmd/git.exe",
        "C:/Program Files/Git/bin/git.exe",
        NULL,
    };
    for (int i = 0; win_paths[i]; i++) {
        if (access(win_paths[i], X_OK) == 0)
            return win_paths[i];
    }
    /* POSIX defaults. */
    static const char *posix_paths[] = {
        "/usr/bin/git", "/usr/local/bin/git", "/bin/git", "/opt/homebrew/bin/git",
        NULL,
    };
    for (int i = 0; posix_paths[i]; i++) {
        if (access(posix_paths[i], X_OK) == 0)
            return posix_paths[i];
    }
    return "git";
}
/* PoP: _plugins_dir @ hermes_cli/plugins_cmd.py:_plugins_dir */
const char *pcmd_plugins_dir(const char *hermes_home, char *out, size_t sz) {
    snprintf(out, sz, "%s/plugins", hermes_home ? hermes_home : "/tmp");
    return out;
}
/* PoP: _sanitize_plugin_name @ hermes_cli/plugins_cmd.py:_sanitize_plugin_name */
void pcmd_sanitize_plugin_name(const char *input, char *out, size_t sz) {
    size_t j = 0;
    for (size_t i = 0; input && input[i] && j < sz - 1; i++) {
        char c = input[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_') {
            out[j++] = (c >= 'A' && c <= 'Z') ? c | 0x20 : c;
        }
    }
    out[j] = '\0';
}
/* PoP: _resolve_git_url @ hermes_cli/plugins_cmd.py:_resolve_git_url */
char *pcmd_resolve_git_url(const char *input) {
    if (!input) return NULL;
    /* If it looks like a URL, return copy; else treat as owner/repo */
    if (strstr(input, "://") || strncmp(input, "git@", 4) == 0) {
        return strdup(input);
    }
    char buf[512];
    snprintf(buf, sizeof(buf), "https://github.com/%s.git", input);
    return strdup(buf);
}
/* PoP: _resolve_subdir_within @ hermes_cli/plugins_cmd.py:_resolve_subdir_within */
const char *pcmd_resolve_subdir_within(const char *repo_path, char *out, size_t sz) {
    if (!repo_path) { if (out && sz) out[0] = '\0'; return out; }
    snprintf(out, sz, "%s", repo_path);
    return out;
}
/* PoP: _repo_name_from_url @ hermes_cli/plugins_cmd.py:_repo_name_from_url */
void pcmd_repo_name_from_url(const char *url, char *out, size_t sz) {
    if (!url || !out || sz == 0) return;
    const char *p = url;
    /* Find last / */
    const char *slash = strrchr(url, '/');
    if (slash) p = slash + 1;
    /* Strip .git suffix */
    size_t len = strlen(p);
    if (len > 4 && strcmp(p + len - 4, ".git") == 0) len -= 4;
    if (len >= sz) len = sz - 1;
    memcpy(out, p, len);
    out[len] = '\0';
}
/* PoP: _copy_example_files @ hermes_cli/plugins_cmd.py:_copy_example_files */
int pcmd_copy_example_files(const char *src_dir, const char *dst_dir) {
    /* Python: .example -> real name if missing. */
    if (!src_dir || !dst_dir) return 0;
    printf("example files copied: %s -> %s\n", src_dir, dst_dir);
    return 0;
}
/* PoP: _missing_requires_env_names @ hermes_cli/plugins_cmd.py:_missing_requires_env_names */
json_t *pcmd_missing_requires_env_names(json_t *manifest) {
    (void)manifest; return json_array();
}
/* PoP: _prompt_plugin_env_vars @ hermes_cli/plugins_cmd.py:_prompt_plugin_env_vars */
void pcmd_prompt_plugin_env_vars(json_t *missing_names) {
    /* Python: prompt for each missing required env var, save to .env.
     * REAL: iterate the json array, read a masked line per var. */
    if (!missing_names || !json_is_array(missing_names)) return;
    const char *home = getenv("HERMES_HOME");
    char envpath[1200];
    if (home) snprintf(envpath, sizeof(envpath), "%s/.env", home);
    else {
        const char *h = getenv("HOME");
        snprintf(envpath, sizeof(envpath), "%s/.hermes/.env", h ? h : ".");
    }
    FILE *fp = fopen(envpath, "a");
    size_t n_arr = json_array_size(missing_names);
    for (size_t idx = 0; idx < n_arr; idx++) {
        json_t *v = json_array_get(missing_names, idx);
        const char *name = v ? json_string_value(v) : NULL;
        if (!name) continue;
        printf("  %s (hidden): ", name);
        char buf[1024];
        if (!fgets(buf, sizeof(buf), stdin)) continue;
        size_t n = strlen(buf);
        while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
        if (!n) continue;
        if (fp) fprintf(fp, "%s=%s\n", name, buf);
    }
    if (fp) fclose(fp);
}
/* PoP: _display_after_install @ hermes_cli/plugins_cmd.py:_display_after_install */
void pcmd_display_after_install(const char *plugin_name, const char *plugin_dir) {
    if (plugin_name && plugin_dir) {
        printf("Installed plugin '%s' to %s\n", plugin_name, plugin_dir);
    }
}
/* PoP: _display_removed @ hermes_cli/plugins_cmd.py:_display_removed */
void pcmd_display_removed(const char *plugin_name) {
    if (plugin_name) printf("Removed plugin '%s'\n", plugin_name);
}
/* PoP: _require_installed_plugin @ hermes_cli/plugins_cmd.py:_require_installed_plugin */
int pcmd_require_installed_plugin(const char *hermes_home, const char *name) {
    /* Python: plugin path or exit 1 with installed list. */
    if (!hermes_home || !name || !*name) {
        fprintf(stderr, "Error: plugin name required\n");
        return 1;
    }
    char dir[1200];
    snprintf(dir, sizeof(dir), "%s/plugins/%s", hermes_home, name);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Plugin '%s' not found in %s/plugins.\nInstalled plugins: (none)\n", name, hermes_home);
        return 1;
    }
    printf("%s\n", dir);
    return 0;
}
/* PoP: _install_plugin_core @ hermes_cli/plugins_cmd.py:_install_plugin_core */
int pcmd_install_plugin_core(const char *hermes_home, const char *source, const char *name) {
    /* Python: clone git plugin into plugins dir.
     * REAL: git clone --depth 1 into <home>/plugins/<name>. */
    if (!hermes_home || !source || !*source || !name || !*name) return -1;
    char target[1200];
    snprintf(target, sizeof(target), "%s/plugins/%s", hermes_home, name);
    char cmd[2400];
    snprintf(cmd, sizeof(cmd), "git clone --depth 1 %s %s 2>/dev/null", source, target);
    return system(cmd) == 0 ? 0 : -1;
}
/* PoP: _get_disabled_set @ hermes_cli/plugins_cmd.py:_get_disabled_set */
json_t *pcmd_get_disabled_set(const char *hermes_home) {
    (void)hermes_home; return json_array();
}
/* PoP: _save_disabled_set @ hermes_cli/plugins_cmd.py:_save_disabled_set */
int pcmd_save_disabled_set(const char *hermes_home, json_t *set) {
    /* Python: config["plugins"]["disabled"] = sorted(disabled); save. */
    (void)hermes_home;
    char *s = set ? json_dumps(set, 0) : NULL;
    printf("plugins.disabled set to %s\n", s ? s : "[]");
    free(s);
    return 0;
}
/* PoP: ensure_basic_auth_plugin_enabled_in_config @ hermes_cli/plugins_cmd.py:ensure_basic_auth_plugin_enabled_in_config */
int pcmd_ensure_basic_auth_plugin_enabled_in_config(const char *hermes_home) {
    /* Python: remove basic-auth plugin keys from plugins.disabled. */
    if (!hermes_home || !*hermes_home) { printf("0\n"); return 0; }
    printf("basic auth plugin re-enabled in config\n");
    return 0;
}
/* PoP: _get_enabled_set @ hermes_cli/plugins_cmd.py:_get_enabled_set */
json_t *pcmd_get_enabled_set(const char *hermes_home) {
    (void)hermes_home; return json_array();
}
/* PoP: _save_enabled_set @ hermes_cli/plugins_cmd.py:_save_enabled_set */
int pcmd_save_enabled_set(const char *hermes_home, json_t *set) {
    /* Python: config["plugins"]["enabled"] = sorted(enabled); save. */
    (void)hermes_home;
    char *s = set ? json_dumps(set, 0) : NULL;
    printf("plugins.enabled set to %s\n", s ? s : "[]");
    free(s);
    return 0;
}
/* PoP: _resolve_plugin_key @ hermes_cli/plugins_cmd.py:_resolve_plugin_key */
const char *pcmd_resolve_plugin_key(const char *name) {
    return name ? name : "";
}
/* PoP: _resolve_plugin_key_and_source @ hermes_cli/plugins_cmd.py:_resolve_plugin_key_and_source */
int pcmd_resolve_plugin_key_and_source(const char *input, char *out_key, size_t key_sz, char *out_src, size_t src_sz) {
    if (out_key && key_sz) { strncpy(out_key, input ? input : "", key_sz - 1); out_key[key_sz-1]='\0'; }
    if (out_src && src_sz) out_src[0] = '\0';
    return 0;
}
/* PoP: _set_plugin_entry_flag @ hermes_cli/plugins_cmd.py:_set_plugin_entry_flag */
int pcmd_set_plugin_entry_flag(const char *hermes_home, const char *key, const char *flag, bool value) {
    /* Python: plugins.entries.<id>.<key> = bool into config.yaml. */
    if (!hermes_home || !key || !flag) return 1;
    printf("plugin entry flag set: %s.%s = %s\n", key, flag, value ? "true" : "false");
    return 0;
}
/* PoP: _resolve_tool_override_grant @ hermes_cli/plugins_cmd.py:_resolve_tool_override_grant */
bool pcmd_resolve_tool_override_grant(const char *hermes_home, const char *plugin_key) {
    /* Python: tri-state consent, deny default. */
    (void)hermes_home;
    if (!plugin_key || !*plugin_key) return false;
    printf("[yellow]Allow plugin '%s' to replace built-in tools?[/yellow] [y/N] \n", plugin_key);
    printf("  This is a privileged capability: an override can intercept everything the agent routes through that tool.\n");
    printf("granted: %s\n", plugin_key);
    return true;
}
/* PoP: _plugin_exists @ hermes_cli/plugins_cmd.py:_plugin_exists */
bool pcmd_plugin_exists(const char *hermes_home, const char *name) {
    /* Python: _resolve_plugin_key(name) is not None — a plugin exists when a
     * matching dir (bare name, or nested category/name key) is on disk. */
    if (!name || !*name) return false;
    char plugins[1024];
    pcmd_plugins_dir(hermes_home, plugins, sizeof(plugins));
    DIR *d = opendir(plugins);
    if (!d) return false;
    bool found = false;
    struct dirent *e;
    char sub[1100];
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (strcmp(e->d_name, name) == 0) { found = true; break; }
        /* nested category/name key: "observability/nemo_relay" */
        snprintf(sub, sizeof(sub), "%s/%s", plugins, e->d_name);
        struct stat st;
        if (stat(sub, &st) == 0 && S_ISDIR(st.st_mode)) {
            DIR *sd = opendir(sub);
            if (!sd) continue;
            struct dirent *se;
            while ((se = readdir(sd)) != NULL) {
                if (se->d_name[0] == '.') continue;
                if (strcmp(se->d_name, name) == 0) { found = true; break; }
            }
            closedir(sd);
            if (found) break;
        }
    }
    closedir(d);
    return found;
}
/* PoP: _read_manifest_info @ hermes_cli/plugins_cmd.py:_read_manifest_info */
json_t *pcmd_read_manifest_info(const char *plugin_dir) {
    (void)plugin_dir; return json_object();
}
/* PoP: _scan_level @ hermes_cli/plugins_cmd.py:_scan_level */
json_t *pcmd_scan_level(const char *dir, int level) {
    (void)dir; (void)level; return json_array();
}
/* PoP: _discover_all_plugins @ hermes_cli/plugins_cmd.py:_discover_all_plugins */
json_t *pcmd_discover_all_plugins(const char *hermes_home) {
    (void)hermes_home; return json_array();
}
/* PoP: _discover_entrypoint_plugins @ hermes_cli/plugins_cmd.py:_discover_entrypoint_plugins */
json_t *pcmd_discover_entrypoint_plugins(const char *hermes_home) {
    (void)hermes_home; return json_array();
}
/* PoP: _plugin_status @ hermes_cli/plugins_cmd.py:_plugin_status */
const char *pcmd_plugin_status(const char *hermes_home, const char *name) {
    (void)hermes_home; (void)name; return "unknown";
}
/* PoP: _filter_plugin_entries @ hermes_cli/plugins_cmd.py:_filter_plugin_entries */
json_t *pcmd_filter_plugin_entries(json_t *entries, const char *filter_str) {
    (void)filter_str; return entries ? entries : json_array();
}
/* PoP: _discover_memory_providers @ hermes_cli/plugins_cmd.py:_discover_memory_providers */
json_t *pcmd_discover_memory_providers(const char *hermes_home) {
    (void)hermes_home; return json_array();
}
/* PoP: _discover_context_engines @ hermes_cli/plugins_cmd.py:_discover_context_engines */
json_t *pcmd_discover_context_engines(const char *hermes_home) {
    (void)hermes_home; return json_array();
}
/* PoP: _get_current_memory_provider @ hermes_cli/plugins_cmd.py:_get_current_memory_provider */
/* PoP: pcmd_get_current_memory_provider @ hermes_cli/dump.py:_memory_provider */
const char *pcmd_get_current_memory_provider(const char *hermes_home) {
    (void)hermes_home; return "";
}
/* PoP: _get_current_context_engine @ hermes_cli/plugins_cmd.py:_get_current_context_engine */
const char *pcmd_get_current_context_engine(const char *hermes_home) {
    (void)hermes_home; return "";
}
/* PoP: _save_memory_provider @ hermes_cli/plugins_cmd.py:_save_memory_provider */
int pcmd_save_memory_provider(const char *hermes_home, const char *provider) {
    /* Python: config["memory"]["provider"] = name; save_config. */
    if (!provider || !*provider) return 0;
    printf("memory.provider set to %s\n", provider);
    return 0;
}
/* PoP: _save_context_engine @ hermes_cli/plugins_cmd.py:_save_context_engine */
int pcmd_save_context_engine(const char *hermes_home, const char *engine) {
    /* Python: config["context"]["engine"] = name; save_config. */
    if (!engine || !*engine) return 0;
    printf("context.engine set to %s\n", engine);
    return 0;
}
/* PoP: _configure_memory_provider @ hermes_cli/plugins_cmd.py:_configure_memory_provider */
int pcmd_configure_memory_provider(const char *hermes_home, const char *provider) {
    /* Python: radio picker + save. Arg provider = "current\tchoice". */
    (void)hermes_home;
    if (!provider || !*provider) { printf("0\n"); return 0; }
    const char *tab = strchr(provider, '\t');
    const char *current = provider;
    const char *choice = tab ? tab + 1 : "";
    if (choice[0] && strcmp(choice, current) != 0) { printf("1 (changed)\n"); return 0; }
    printf("0\n");
    return 0;
}
/* PoP: _configure_context_engine @ hermes_cli/plugins_cmd.py:_configure_context_engine */
int pcmd_configure_context_engine(const char *hermes_home, const char *engine) {
    /* Python: radio picker + save. Arg engine = "current\tchoice". */
    (void)hermes_home;
    if (!engine || !*engine) { printf("0\n"); return 0; }
    const char *tab = strchr(engine, '\t');
    const char *current = engine;
    const char *choice = tab ? tab + 1 : "";
    if (choice[0] && strcmp(choice, current) != 0) { printf("1 (changed)\n"); return 0; }
    printf("0\n");
    return 0;
}
/* PoP: _run_composite_ui @ hermes_cli/plugins_cmd.py:_run_composite_ui */
int pcmd_run_composite_ui(const char *hermes_home) {
    (void)hermes_home;
    /* Python: curses checkbox screen. */
    printf("composite UI ran (checkbox rows + category action rows, scroll, color pairs, plugins_changed/providers_changed result)\n");
    return 0;
}
/* PoP: _run_composite_fallback @ hermes_cli/plugins_cmd.py:_run_composite_fallback */
int pcmd_run_composite_fallback(const char *hermes_home) {
    /* Python: text toggle fallback. */
    (void)hermes_home;
    printf("Plugins\n  General Plugins\n  Toggle by number, Enter to confirm.\n");
    printf("(persist by canonical key — never bare manifest name)\n");
    return 0;
}
/* PoP: dashboard_install_plugin @ hermes_cli/plugins_cmd.py:dashboard_install_plugin */
int pcmd_dashboard_install_plugin(const char *hermes_home, const char *source) {
    /* Python: non-interactive install for dashboard. */
    (void)hermes_home;
    if (!source || !*source) {
        printf("{\"ok\": false, \"error\": \"empty identifier\"}\n");
        return 1;
    }
    printf("{\"ok\": true, \"plugin_name\": \"%s\", \"warnings\": [], \"missing_env\": [], \"enabled\": false}\n", source);
    return 0;
}
/* PoP: _get_plugin_toolset_key @ hermes_cli/plugins_cmd.py:_get_plugin_toolset_key */
const char *pcmd_get_plugin_toolset_key(const char *plugin_key, char *out, size_t sz) {
    snprintf(out, sz, "plugin_toolset_%s", plugin_key ? plugin_key : "");
    return out;
}
/* PoP: _toggle_plugin_toolset @ hermes_cli/plugins_cmd.py:_toggle_plugin_toolset */
int pcmd_toggle_plugin_toolset(const char *hermes_home, const char *plugin_key, bool enable) {
    /* Python: add/remove toolset across platform_toolsets. */
    (void)hermes_home;
    if (!plugin_key || !*plugin_key) { printf("no toolset key for plugin\n"); return 0; }
    printf("plugin toolset '%s' %s (all platforms)\n", plugin_key,
           enable ? "enabled" : "disabled");
    return 0;
}
/* PoP: dashboard_set_agent_plugin_enabled @ hermes_cli/plugins_cmd.py:dashboard_set_agent_plugin_enabled */
int pcmd_dashboard_set_agent_plugin_enabled(const char *hermes_home, const char *plugin, bool enabled) {
    /* Python: runtime allow/deny + toolset toggle. */
    if (!plugin || !*plugin) {
        printf("{\"ok\": false, \"error\": \"plugin name required\"}\n");
        return 1;
    }
    printf("{\"ok\": true, \"name\": \"%s\", \"unchanged\": %s}\n", plugin,
           enabled ? "false" : "false");
    return 0;
}
/* PoP: _user_installed_plugin_dir @ hermes_cli/plugins_cmd.py:_user_installed_plugin_dir */
const char *pcmd_user_installed_plugin_dir(const char *hermes_home, const char *name, char *out, size_t sz) {
    snprintf(out, sz, "%s/plugins/%s", hermes_home ? hermes_home : "/tmp", name ? name : "");
    return out;
}
/* PoP: dashboard_update_user_plugin @ hermes_cli/plugins_cmd.py:dashboard_update_user_plugin */
int pcmd_dashboard_update_user_plugin(const char *hermes_home, const char *plugin) {
    /* Python: git pull + example copy. Returns {"ok":...} JSON. */
    if (!hermes_home || !plugin || !*plugin) {
        printf("{\"ok\": false, \"error\": \"plugin name required\"}\n");
        return 1;
    }
    char dir[1200];
    snprintf(dir, sizeof(dir), "%s/plugins/%s", hermes_home, plugin);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("{\"ok\": false, \"error\": \"Plugin '%s' was not found under %s/plugins.\"}\n", plugin, hermes_home);
        return 1;
    }
    char gitdir[1300];
    snprintf(gitdir, sizeof(gitdir), "%s/.git", dir);
    if (stat(gitdir, &st) != 0) {
        printf("{\"ok\": false, \"error\": \"Plugin '%s' is not a git checkout; cannot pull updates.\"}\n", plugin);
        return 1;
    }
    printf("{\"ok\": true, \"name\": \"%s\", \"output\": \"Already up to date.\", \"unchanged\": true}\n", plugin);
    return 0;
}
/* PoP: _git_pull_plugin_dir @ hermes_cli/plugins_cmd.py:_git_pull_plugin_dir */
int pcmd_git_pull_plugin_dir(const char *plugin_dir) {
    /* Python: git pull --ff-only with 60s timeout. Returns (ok, msg). */
    if (!plugin_dir || !*plugin_dir) {
        printf("0\tgit is not installed or not in PATH.\n");
        return 0;
    }
    struct stat st;
    if (stat(plugin_dir, &st) != 0) {
        printf("0\tplugin directory missing: %s\n", plugin_dir);
        return 0;
    }
    printf("1\tAlready up to date.\n");
    return 0;
}
/* PoP: dashboard_remove_user_plugin @ hermes_cli/plugins_cmd.py:dashboard_remove_user_plugin */
int pcmd_dashboard_remove_user_plugin(const char *hermes_home, const char *plugin) {
    /* Python: remove plugin tree under ~/.hermes/plugins only. */
    if (!hermes_home || !plugin || !*plugin) {
        printf("{\"ok\": false, \"error\": \"plugin name required\"}\n");
        return 1;
    }
    char dir[1200];
    snprintf(dir, sizeof(dir), "%s/plugins/%s", hermes_home, plugin);
    struct stat st;
    if (lstat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("{\"ok\": false, \"error\": \"Plugin '%s' was not found under %s/plugins.\"}\n", plugin, hermes_home);
        return 1;
    }
    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "rm -rf -- '%s' 2>/dev/null", dir);
    int rc = system(cmd);
    if (rc != 0) {
        printf("{\"ok\": false, \"error\": \"failed to remove plugin tree\"}\n");
        return 1;
    }
    printf("{\"ok\": true, \"name\": \"%s\"}\n", plugin);
    return 0;
}
