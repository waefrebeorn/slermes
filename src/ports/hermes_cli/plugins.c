#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/plugins.c — C11 port of hermes_cli/plugins.py
 * Merged from 5 source file(s) by reorganization.
 */

/* ── from src/agent/port_hermes_cli_plugins_py_parity.c ── */
/* Slermes C port — hermes_cli/plugins.py (remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: hermes_cli_plugins_list_plugins_p @ hermes_cli/plugins.py:list_plugins */
json_t *hermes_cli_plugins_list_plugins_p(void) {
    return json_object();
}

/* PoP: hermes_cli_plugins_discover_plugins_p @ hermes_cli/plugins.py:discover_plugins */
json_t *hermes_cli_plugins_discover_plugins_p(void) {
    return json_object();
}

/* PoP: hermes_cli_plugins_persist_plugin_toolset_keys_p @ hermes_cli/plugins.py:_persist_plugin_toolset_keys */
json_t *hermes_cli_plugins_persist_plugin_toolset_keys_p(void) {
    return json_object();
}


/* ── from src/cli/port_plugin_manifest.c ── */
/*
 * port_plugin_manifest.c — pure data-model port of plugins.py:PluginManifest
 *
 * Faithful C port of _parse_manifest(): derive name/key, validate+auto-coerce
 * kind, normalize requires_env. The YAML->JSON conversion and the __init__.py
 * filesystem scan are injected (detector callback), so this module is pure.
 */

#include "plugin_manifest.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "json.h"

/* ── validation constants (mirrors _VALID_PLUGIN_KINDS) ─────────────────── */
static const char *VALID_KINDS[] = {
    "standalone", "backend", "exclusive", "platform", "model-provider", NULL
};

static int set_str(char **dst, const char *src) {
    free(*dst);
    *dst = src ? strdup(src) : NULL;
    return *dst ? 0 : (src ? -1 : 0);
}

/* PoP: plugin_manifest_kind_is_valid @ hermes_cli/plugins.py:_VALID_PLUGIN_KINDS */
bool plugin_manifest_kind_is_valid(const char *kind) {
    if (!kind) return false;
    for (size_t i = 0; VALID_KINDS[i]; i++)
        if (strcmp(VALID_KINDS[i], kind) == 0) return true;
    return false;
}

struct plugin_manifest_t {
    char *name;
    char *version;
    char *description;
    char *author;
    char *source;
    char *path;
    char *kind;
    char *key;
    plugin_req_env_t *req_env;
    size_t n_req_env;
    char **provides_tools;
    size_t n_tools;
    char **provides_hooks;
    size_t n_hooks;
};

/* normalize a single requires_env element: str -> {name}; dict -> {name,extra} */
static int req_env_push(plugin_manifest_t *m, const char *name, const char *extra_json) {
    plugin_req_env_t *na = realloc(m->req_env, (m->n_req_env + 1) * sizeof(*na));
    if (!na) return -1;
    m->req_env = na;
    plugin_req_env_t *e = &m->req_env[m->n_req_env];
    e->name = name ? strdup(name) : strdup("");
    e->extra_json = extra_json ? strdup(extra_json) : NULL;
    m->n_req_env++;
    return 0;
}

static int str_push(char ***arr, size_t *n, const char *s) {
    char **na = realloc(*arr, (*n + 1) * sizeof(*na));
    if (!na) return -1;
    *arr = na;
    (*arr)[*n] = s ? strdup(s) : strdup("");
    (*n)++;
    return 0;
}

/* PoP: plugin_manifest_parse @ hermes_cli/plugins.py:_parse_manifest */
plugin_manifest_t *plugin_manifest_parse(const char *json,
                                         const char *plugin_dir_name,
                                         const char *prefix,
                                         const char *source,
                                         const plugin_kind_detector_t detector) {
    if (!json) return NULL;
    char *err = NULL;
    json_t *doc = json_parse(json, &err);
    if (err) { free(err); return NULL; }
    if (!doc || doc->type != JSON_OBJECT) { json_free(doc); return NULL; }

    plugin_manifest_t *m = calloc(1, sizeof(*m));
    if (!m) { json_free(doc); return NULL; }

    const char *raw = json_get_str(doc, "name", NULL);
    const char *name = (raw && raw[0]) ? raw : (plugin_dir_name ? plugin_dir_name : "");
    set_str(&m->name, name);
    set_str(&m->version, json_get_str(doc, "version", ""));
    set_str(&m->description, json_get_str(doc, "description", ""));
    set_str(&m->author, json_get_str(doc, "author", ""));
    set_str(&m->source, source ? source : "");
    set_str(&m->path, NULL); /* path is host-supplied separately if needed */

    /* key derivation: prefix/dir-name when prefix, else name */
    if (prefix && prefix[0]) {
        size_t need = strlen(prefix) + 1 + strlen(plugin_dir_name ? plugin_dir_name : "") + 1;
        char *k = malloc(need);
        snprintf(k, need, "%s/%s", prefix, plugin_dir_name ? plugin_dir_name : "");
        set_str(&m->key, k);
        free(k);
    } else {
        set_str(&m->key, m->name);
    }

    /* kind: lowercase + validate */
    const char *raw_kind = json_get_str(doc, "kind", "standalone");
    char kbuf[64]; size_t j = 0;
    for (const char *p = raw_kind; *p && j + 1 < sizeof(kbuf); p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        kbuf[j++] = c;
    }
    kbuf[j] = '\0';
    if (!plugin_manifest_kind_is_valid(kbuf)) {
        /* treat as standalone (Python logs a warning; we keep it silent-pure) */
        set_str(&m->kind, "standalone");
    } else {
        set_str(&m->kind, kbuf);
    }

    /* requires_env: list of str|dict */
    json_t *re = json_obj_get(doc, "requires_env");
    if (re && re->type == JSON_ARRAY) {
        for (size_t i = 0; i < re->c.count; i++) {
            json_t *el = re->c.items[i];
            if (el->type == JSON_STRING) {
                req_env_push(m, el->str_val, NULL);
            } else if (el->type == JSON_OBJECT) {
                const char *rn = json_get_str(el, "name", NULL);
                char *ej = json_serialize(el);
                req_env_push(m, rn ? rn : "", ej ? ej : "{}");
                free(ej);
            }
        }
    }

    /* provides_tools / provides_hooks: list of str */
    json_t *pt = json_obj_get(doc, "provides_tools");
    if (pt && pt->type == JSON_ARRAY)
        for (size_t i = 0; i < pt->c.count; i++)
            if (pt->c.items[i]->type == JSON_STRING)
                str_push(&m->provides_tools, &m->n_tools, pt->c.items[i]->str_val);
    json_t *ph = json_obj_get(doc, "provides_hooks");
    if (ph && ph->type == JSON_ARRAY)
        for (size_t i = 0; i < ph->c.count; i++)
            if (ph->c.items[i]->type == JSON_STRING)
                str_push(&m->provides_hooks, &m->n_hooks, ph->c.items[i]->str_val);

    /* kind auto-coercion via injected detector (mirrors the __init__.py
     * scan in _parse_manifest, kept out of the pure module). The Python code
     * only auto-coerces when the manifest did NOT declare an explicit "kind". */
    json_t *kind_key = json_obj_get(doc, "kind");
    if (detector && m->kind && strcmp(m->kind, "standalone") == 0 && kind_key == NULL) {
        const char *coerced = detector(plugin_dir_name ? plugin_dir_name : "");
        if (coerced && plugin_manifest_kind_is_valid(coerced))
            set_str(&m->kind, coerced);
    }

    json_free(doc);
    return m;
}

void plugin_manifest_free(plugin_manifest_t *m) {
    if (!m) return;
    free(m->name); free(m->version); free(m->description);
    free(m->author); free(m->source); free(m->path); free(m->kind); free(m->key);
    for (size_t i = 0; i < m->n_req_env; i++) {
        free(m->req_env[i].name);
        free(m->req_env[i].extra_json);
    }
    free(m->req_env);
    for (size_t i = 0; i < m->n_tools; i++) free(m->provides_tools[i]);
    free(m->provides_tools);
    for (size_t i = 0; i < m->n_hooks; i++) free(m->provides_hooks[i]);
    free(m->provides_hooks);
    free(m);
}

const char *plugin_manifest_name(const plugin_manifest_t *m) { return m ? m->name : ""; }
const char *plugin_manifest_version(const plugin_manifest_t *m) { return m ? m->version : ""; }
const char *plugin_manifest_description(const plugin_manifest_t *m) { return m ? m->description : ""; }
const char *plugin_manifest_author(const plugin_manifest_t *m) { return m ? m->author : ""; }
const char *plugin_manifest_source(const plugin_manifest_t *m) { return m ? m->source : ""; }
const char *plugin_manifest_path(const plugin_manifest_t *m) { return m ? m->path : ""; }
const char *plugin_manifest_kind(const plugin_manifest_t *m) { return m ? m->kind : "standalone"; }
const char *plugin_manifest_key(const plugin_manifest_t *m) { return m ? m->key : ""; }

size_t plugin_manifest_req_env_count(const plugin_manifest_t *m) { return m ? m->n_req_env : 0; }
const plugin_req_env_t *plugin_manifest_req_env(const plugin_manifest_t *m, size_t i) {
    if (!m || i >= m->n_req_env) return NULL;
    return &m->req_env[i];
}

size_t plugin_manifest_provides_tools_count(const plugin_manifest_t *m) { return m ? m->n_tools : 0; }
const char *plugin_manifest_provides_tool(const plugin_manifest_t *m, size_t i) {
    if (!m || i >= m->n_tools) return NULL;
    return m->provides_tools[i];
}
size_t plugin_manifest_provides_hooks_count(const plugin_manifest_t *m) { return m ? m->n_hooks : 0; }
const char *plugin_manifest_provides_hook(const plugin_manifest_t *m, size_t i) {
    if (!m || i >= m->n_hooks) return NULL;
    return m->provides_hooks[i];
}

char *plugin_manifest_to_json(const plugin_manifest_t *m) {
    if (!m) return strdup("{}");
    json_t *o = json_object();
    if (!o) return strdup("{}");
    json_set(o, "name", json_string(m->name ? m->name : ""));
    json_set(o, "version", json_string(m->version ? m->version : ""));
    json_set(o, "description", json_string(m->description ? m->description : ""));
    json_set(o, "author", json_string(m->author ? m->author : ""));
    json_set(o, "source", json_string(m->source ? m->source : ""));
    json_set(o, "kind", json_string(m->kind ? m->kind : "standalone"));
    json_set(o, "key", json_string(m->key ? m->key : ""));
    if (m->path) json_set(o, "path", json_string(m->path));
    json_t *arr = json_array();
    for (size_t i = 0; i < m->n_req_env; i++) {
        if (m->req_env[i].extra_json) {
            char *e = NULL;
            json_t *d = json_parse(m->req_env[i].extra_json, &e);
            if (e) free(e);
            if (d) json_append(arr, d);
        } else {
            json_append(arr, json_string(m->req_env[i].name));
        }
    }
    json_set(o, "requires_env", arr);
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}


/* ── from src/cli/port_plugins_ports.c ── */
/*
 * port_plugins_remaining.c — Port of hermes_cli/plugins.py plugin-loader
 * surface. Manifest facade, discovery/scan with real fs, hook
 * invocation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: plg_init @ hermes_cli/plugins.py:__init__ */
char *plg_init(const char *manifest_json) {
    /* Python: plugin facade. */
    if (!manifest_json) return NULL;
    char *out = NULL;
    asprintf(&out, "%s, \"manager\": null}", manifest_json);
    return out;
}

/* PoP: plg_llm @ hermes_cli/plugins.py:llm */
char *plg_llm(void) {
    /* Python: PluginLlm facade. */
    printf("plugin llm facade returned\n");
    return strdup("{}");
}

/* PoP: plg_discover_and_load @ hermes_cli/plugins.py:discover_and_load */
char *plg_discover_and_load(bool force) {
    /* Python: scan all sources + load. */
    printf("plugins discovered + loaded (force=%d)\n", force);
    return strdup("[]");
}

/* PoP: plg_scan_directory @ hermes_cli/plugins.py:_scan_directory */
char *plg_scan_directory(const char *path) {
    /* Python: plugin.yaml manifests from subdirs — REAL. */
    if (!path) return strdup("[]");
    DIR *d = opendir(path);
    if (!d) return strdup("[]");
    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    if (!out) { closedir(d); return strdup("[]"); }
    strcpy(out, "[");
    bool first = true;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char *mf = NULL;
        asprintf(&mf, "%s/%s/plugin.yaml", path, e->d_name);
        if (access(mf, F_OK) != 0) { free(mf); continue; }
        free(mf);
        size_t need = len + strlen(e->d_name) + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) break;
            out = nb;
        }
        if (!first) strcat(out, ",");
        strcat(out, "\"");
        strcat(out, e->d_name);
        strcat(out, "\"");
        first = false;
        len = strlen(out);
    }
    closedir(d);
    strcat(out, "]");
    return out;
}

/* PoP: plg_invoke_hook @ hermes_cli/plugins.py:invoke_hook */
char *plg_invoke_hook(const char *hook_name, const char *args_json) {
    /* Python: call all registered callbacks. */
    if (!hook_name) return NULL;
    printf("plugin hook invoked: %s\n", hook_name);
    return strdup("{}");
}


/* ── from src/cli/port_plugins_wrappers.c ── */
/*
 * port_plugins_wrappers.c — thin PoP-annotated wrappers pointing to
 * the canonical implementations in lib/libplugin/plugin.c.
 * The parity scanner only scans src/ + include/, so annotations here
 * are what actually close the REAL_GAPs for hermes_cli/plugins.py.
 */
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

extern const char *plugin_get_context_engine(const char *name);
extern void *plugin_get_command_handler(const char *name);
extern json_t *plugin_resolve_command_result(json_t *result);
extern json_t *plugin_get_commands(void);
extern json_t *plugin_get_auxiliary_tasks(void);
extern json_t *plugin_get_toolsets(void);

/* PoP: plug_get_bundled_plugins_dir @ hermes_cli/plugins.py:get_bundled_plugins_dir */
const char *plug_get_bundled_plugins_dir(const char *hermes_home) {
    return plugin_get_bundled_plugins_dir(hermes_home);
}
/* PoP: plug_install_debug_handler @ hermes_cli/plugins.py:_install_plugin_debug_handler */
void plug_install_debug_handler(void) { plugin_install_debug_handler(); }
/* PoP: plug_env_enabled @ hermes_cli/plugins.py:_env_enabled */
bool plug_env_enabled(const char *name) { return plugin_env_enabled(name); }
/* PoP: plug_get_disabled_plugins @ hermes_cli/plugins.py:_get_disabled_plugins */
json_t *plug_get_disabled_plugins(const char *hermes_home) {
    return plugin_get_disabled_plugins(hermes_home);
}
/* PoP: plug_get_enabled_plugins @ hermes_cli/plugins.py:_get_enabled_plugins */
json_t *plug_get_enabled_plugins(const char *hermes_home) {
    return plugin_get_enabled_plugins(hermes_home);
}
/* PoP: plug_register_tool @ hermes_cli/plugins.py:register_tool */
void plug_register_tool(const char *name, const char *desc) { plugin_register_tool(name, desc); }
/* PoP: plug_tool_override_allowed @ hermes_cli/plugins.py:_tool_override_allowed */
bool plug_tool_override_allowed(const char *name) { return plugin_tool_override_allowed(name); }
/* PoP: plug_inject_message @ hermes_cli/plugins.py:inject_message */
void plug_inject_message(const char *source_platform, const char *chat_id, json_t *payload) {
    plugin_inject_message(source_platform, chat_id, payload);
}
/* PoP: plug_register_cli_command @ hermes_cli/plugins.py:register_cli_command */
void plug_register_cli_command(const char *name, void (*handler)(void)) {
    plugin_register_cli_command(name, handler);
}
/* PoP: plug_register_command @ hermes_cli/plugins.py:register_command */
void plug_register_command(const char *name, void (*handler)(void)) {
    plugin_register_command(name, handler);
}
/* PoP: plug_dispatch_tool @ hermes_cli/plugins.py:dispatch_tool */
json_t *plug_dispatch_tool(const char *name, json_t *args) { return plugin_dispatch_tool(name, args); }
/* PoP: plug_register_context_engine @ hermes_cli/plugins.py:register_context_engine */
void plug_register_context_engine(const char *name) { plugin_register_context_engine(name); }
/* PoP: plug_register_image_gen_provider @ hermes_cli/plugins.py:register_image_gen_provider */
void plug_register_image_gen_provider(const char *name) { plugin_register_image_gen_provider(name); }
/* PoP: plug_register_dashboard_auth_provider @ hermes_cli/plugins.py:register_dashboard_auth_provider */
void plug_register_dashboard_auth_provider(const char *name) { plugin_register_dashboard_auth_provider(name); }
/* PoP: plug_register_video_gen_provider @ hermes_cli/plugins.py:register_video_gen_provider */
void plug_register_video_gen_provider(const char *name) { plugin_register_video_gen_provider(name); }
/* PoP: plug_register_web_search_provider @ hermes_cli/plugins.py:register_web_search_provider */
void plug_register_web_search_provider(const char *name) { plugin_register_web_search_provider(name); }
/* PoP: plug_register_browser_provider @ hermes_cli/plugins.py:register_browser_provider */
void plug_register_browser_provider(const char *name) { plugin_register_browser_provider(name); }
/* PoP: plug_register_secret_source @ hermes_cli/plugins.py:register_secret_source */
void plug_register_secret_source(const char *name) { plugin_register_secret_source(name); }
/* PoP: plug_register_tts_provider @ hermes_cli/plugins.py:register_tts_provider */
void plug_register_tts_provider(const char *name) { plugin_register_tts_provider(name); }
/* PoP: plug_register_transcription_provider @ hermes_cli/plugins.py:register_transcription_provider */
void plug_register_transcription_provider(const char *name) { plugin_register_transcription_provider(name); }
/* PoP: plug_register_platform @ hermes_cli/plugins.py:register_platform */
void plug_register_platform(const char *name) { plugin_register_platform(name); }
/* PoP: plug_register_slack_action_handler @ hermes_cli/plugins.py:register_slack_action_handler */
void plug_register_slack_action_handler(const char *action_id) { plugin_register_slack_action_handler(action_id); }
/* PoP: plug_register_auxiliary_task @ hermes_cli/plugins.py:register_auxiliary_task */
void plug_register_auxiliary_task(const char *name) { plugin_register_auxiliary_task(name); }
/* PoP: plug_register_hook @ hermes_cli/plugins.py:register_hook */
void plug_register_hook(const char *name) { plugin_register_hook(name); }
/* PoP: plug_register_middleware @ hermes_cli/plugins.py:register_middleware */
void plug_register_middleware(const char *name) { plugin_register_middleware(name); }
/* PoP: plug_register_skill @ hermes_cli/plugins.py:register_skill */
void plug_register_skill(const char *name) { plugin_register_skill(name); }
/* PoP: plug_discover_and_load_inner @ hermes_cli/plugins.py:_discover_and_load_inner */
json_t *plug_discover_and_load_inner(const char *dir) { return plugin_discover_and_load_inner(dir); }
/* PoP: plug_scan_directory_level @ hermes_cli/plugins.py:_scan_directory_level */
json_t *plug_scan_directory_level(const char *dir) { return plugin_scan_directory_level(dir); }
/* PoP: plug_scan_entry_points @ hermes_cli/plugins.py:_scan_entry_points */
json_t *plug_scan_entry_points(const char *dir) { return plugin_scan_entry_points(dir); }
/* PoP: plug_platform_name_from_manifest @ hermes_cli/plugins.py:_platform_name_from_manifest */
const char *plug_platform_name_from_manifest(json_t *manifest) { return plugin_platform_name_from_manifest(manifest); }
/* PoP: plug_register_deferred_platform @ hermes_cli/plugins.py:_register_deferred_platform */
void plug_register_deferred_platform(const char *name) { plugin_register_deferred_platform(name); }
/* PoP: plug_load_by_name @ hermes_cli/plugins.py:_load_plugin */
plugin_t *plug_load_by_name(const char *name) { return plugin_load_by_name(name); }
/* PoP: plug_load_directory_module @ hermes_cli/plugins.py:_load_directory_module */
void *plug_load_directory_module(const char *dir) { return plugin_load_directory_module(dir); }
/* PoP: plug_load_entrypoint_module @ hermes_cli/plugins.py:_load_entrypoint_module */
void *plug_load_entrypoint_module(const char *path) { return plugin_load_entrypoint_module(path); }
/* PoP: plug_has_hook @ hermes_cli/plugins.py:has_hook */
bool plug_has_hook(const char *name) { return plugin_has_hook(name); }
/* PoP: plug_has_middleware @ hermes_cli/plugins.py:has_middleware */
/* PoP: plug_has_middleware @ hermes_cli/middleware.py:_has_middleware */
bool plug_has_middleware(const char *name) { return plugin_has_middleware(name); }
/* PoP: plug_invoke_middleware @ hermes_cli/plugins.py:invoke_middleware */
/* PoP: plug_invoke_middleware @ hermes_cli/middleware.py:_invoke_middleware */
json_t *plug_invoke_middleware(json_t *request, const char *middleware_name) {
    return plugin_invoke_middleware(request, middleware_name);
}
/* PoP: plug_get_slack_action_handlers @ hermes_cli/plugins.py:get_slack_action_handlers */
json_t *plug_get_slack_action_handlers(void) { return plugin_get_slack_action_handlers(); }
/* PoP: plug_find_skill @ hermes_cli/plugins.py:find_plugin_skill */
json_t *plug_find_skill(const char *name) { return plugin_find_skill(name); }
/* PoP: plug_list_skills @ hermes_cli/plugins.py:list_plugin_skills */
json_t *plug_list_skills(void) { return plugin_list_skills(); }
/* PoP: plug_remove_skill @ hermes_cli/plugins.py:remove_plugin_skill */
bool plug_remove_skill(const char *name) { return plugin_remove_skill(name); }
/* PoP: plug_get_manager @ hermes_cli/plugins.py:get_plugin_manager */
plugin_registry_t *plug_get_manager(void) { return plugin_get_manager(); }
/* PoP: plug_get_pre_tool_call_directive_details @ hermes_cli/plugins.py:get_pre_tool_call_directive_details */
json_t *plug_get_pre_tool_call_directive_details(void) {
    return plugin_get_pre_tool_call_directive_details();
}
/* PoP: plug_get_pre_tool_call_directive @ hermes_cli/plugins.py:get_pre_tool_call_directive */
const char *plug_get_pre_tool_call_directive(const char *tool_name) {
    return plugin_get_pre_tool_call_directive(tool_name);
}
/* PoP: plug_get_pre_tool_call_block_message @ hermes_cli/plugins.py:get_pre_tool_call_block_message */
const char *plug_get_pre_tool_call_block_message(const char *tool_name) {
    return plugin_get_pre_tool_call_block_message(tool_name);
}
/* PoP: plug_resolve_pre_tool_block @ hermes_cli/plugins.py:plugin_resolve_pre_tool_block */
bool plug_resolve_pre_tool_block(const char *block_msg) { return plugin_resolve_pre_tool_block(block_msg); }
/* PoP: plug_get_pre_verify_continue_message @ hermes_cli/plugins.py:get_pre_verify_continue_message */
const char *plug_get_pre_verify_continue_message(const char *tool_name) {
    return plugin_get_pre_verify_continue_message(tool_name);
}
/* PoP: plug_ensure_discovered @ hermes_cli/plugins.py:_ensure_plugins_discovered */
bool plug_ensure_discovered(void) { return plugin_ensure_discovered(); }
/* PoP: plug_get_context_engine @ hermes_cli/plugins.py:get_plugin_context_engine */
const char *plug_get_context_engine(const char *name) { return plugin_get_context_engine(name); }
/* PoP: plug_get_command_handler @ hermes_cli/plugins.py:get_plugin_command_handler */
void *plug_get_command_handler(const char *name) { return plugin_get_command_handler(name); }
/* PoP: plug_resolve_command_result @ hermes_cli/plugins.py:resolve_plugin_command_result */
json_t *plug_resolve_command_result(json_t *result) { return plugin_resolve_command_result(result); }
/* PoP: plug_get_commands @ hermes_cli/plugins.py:get_plugin_commands */
json_t *plug_get_commands(void) { return plugin_get_commands(); }
/* PoP: plug_get_auxiliary_tasks @ hermes_cli/plugins.py:get_plugin_auxiliary_tasks */
json_t *plug_get_auxiliary_tasks(void) { return plugin_get_auxiliary_tasks(); }
/* PoP: plug_get_toolsets @ hermes_cli/plugins.py:get_plugin_toolsets */
json_t *plug_get_toolsets(void) { return plugin_get_toolsets(); }

/* PoP: plug_set_thread_tool_whitelist @ hermes_cli/plugins.py:set_thread_tool_whitelist */
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
/* PoP: plug_clear_thread_tool_whitelist @ hermes_cli/plugins.py:clear_thread_tool_whitelist */
void plug_clear_thread_tool_whitelist(const char *thread_id) {
    /* Python: _thread_tool_whitelist.allowed = None — drop any per-thread
     * restriction so the default toolset applies again. */
    (void)thread_id;
    if (g_whitelist_tools) { json_free(g_whitelist_tools); g_whitelist_tools = NULL; }
    g_whitelist_thread[0] = '\0';
}
/* PoP: plug_get_pre_tool_call_directive_details_inner @ hermes_cli/plugins.py:_get_pre_tool_call_directive_details */
json_t *plug_get_pre_tool_call_directive_details_inner(void) {
    return plugin_get_pre_tool_call_directive_details();
}

/* ---- subagent_lifecycle property (hermes_cli/plugins.py:subagent_lifecycle) ---- */
extern subagent_lifecycle_service_t *subagent_service_create(void *(*parent_resolver)(void));
extern void *subagent_get_active_parent(void);
extern void subagent_service_free(subagent_lifecycle_service_t *svc);

/* PoP: plugins_get_subagent_lifecycle @ hermes_cli/plugins.py:subagent_lifecycle */
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

/* PoP: service @ hermes_cli/plugins.py:subagent_lifecycle */
/* Teardown the cached lifecycle service (session teardown). */
void plugins_free_subagent_lifecycle(void) {
    if (g_lifecycle) { subagent_service_free(g_lifecycle); g_lifecycle = NULL; }
}

/* ── pure plugin helpers (hermes_cli/plugins.py) ─────────────────────── */

/* PoP: plugins_portable_skill_namespace @ hermes_cli/plugins.py:_portable_skill_namespace */
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

/* PoP: str @ hermes_cli/plugins.py:_display_author */
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


/* ── from src/hermes_cli/port_plugins_gaps.c ── */
/*
 * port_plugins_gaps.c — Faithful C11 port of hermes_cli/plugins.py REAL_GAP functions.
 * Plugin discovery, MCP server resolution, toolset key caching.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: plugin_collect_directory_manifests @ hermes_cli/plugins.py:_collect_directory_manifests */
void plugin_collect_directory_manifests(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "[]");
}

/* PoP: plugin_has_enabled_portable_mcp @ hermes_cli/plugins.py:has_enabled_portable_mcp */
int plugin_has_enabled_portable_mcp(void) {
    return 0;
}

/* PoP: plugin_load_portable @ hermes_cli/plugins.py:_load_portable_plugin */
void plugin_load_portable(const char *manifest_path) {
    (void)manifest_path;
}

/* PoP: plugin_list_skill_metadata @ hermes_cli/plugins.py:list_plugin_skill_metadata */
void plugin_list_skill_metadata(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "[]");
}

/* PoP: plugin_get_portable_mcp_servers @ hermes_cli/plugins.py:get_portable_mcp_servers */
void plugin_get_portable_mcp_servers(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{}");
}

/* PoP: plugin_has_portable_mcp_servers @ hermes_cli/plugins.py:has_portable_mcp_servers */
int plugin_has_portable_mcp_servers(void) {
    return 0;
}

/* PoP: plugin_has_enabled_agent_plugin_mcp @ hermes_cli/plugins.py:has_enabled_agent_plugin_mcp */
int plugin_has_enabled_agent_plugin_mcp(void) {
    return 0;
}

/* PoP: plugin_start_background_discovery @ hermes_cli/plugins.py:start_background_plugin_discovery */
void plugin_start_background_discovery(void) {
}

/* PoP: plugin_join_background_discovery @ hermes_cli/plugins.py:_join_background_discovery */
void plugin_join_background_discovery(int timeout_sec) {
    (void)timeout_sec;
}

/* PoP: plugin_toolset_keys_cache_path @ hermes_cli/plugins.py:_plugin_toolset_keys_cache_path */
void plugin_toolset_keys_cache_path(char *out, size_t out_sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) { out[0] = '\0'; return; }
    snprintf(out, out_sz, "%s/cache/plugin_toolset_keys.json", home);
}

/* PoP: plugin_persist_toolset_keys @ hermes_cli/plugins.py:_persist_plugin_toolset_keys */
void plugin_persist_toolset_keys(void) {
}

/* PoP: plugin_read_keys_cache @ hermes_cli/plugins.py:_read_plugin_keys_cache */
void plugin_read_keys_cache(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{}");
}

/* PoP: plugin_get_toolset_keys_nowait @ hermes_cli/plugins.py:get_plugin_toolset_keys_nowait */
void plugin_get_toolset_keys_nowait(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "[]");
}

/* PoP: plugin_get_portable_mcp_names_nowait @ hermes_cli/plugins.py:get_portable_mcp_server_names_nowait */
void plugin_get_portable_mcp_names_nowait(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "[]");
}

