/* tools_config_toolsets.c — faithful C11 port of the toolset-enumeration and
 * toolset-state surface of hermes_cli/tools_config.py.
 *
 * Reuses (does NOT re-implement):
 *   platform_tools.h            — _get_platform_tools / _save_platform_tools /
 *                                 CONFIGURABLE_TOOLSETS keys / PLATFORMS
 *   tools_config_helpers.h      — _toolset_allowed_for_platform
 *   registry.h                  — the tool registry (schemas, tool names)
 *   hermes_tokenizer.h          — cl100k_base-calibrated token counting
 *   plugins_core.h              — plugin manifests + the toolset-key cache
 */
#define _GNU_SOURCE
#include "tools_config_toolsets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "platform_tools.h"
#include "tools_config_helpers.h"
#include "registry.h"
#include "hermes_tokenizer.h"
#include "plugins_core.h"

/* env resolution shared with the rest of the config surface
 * (port_config_py_helpers.c) — declared here to keep includes minimal. */
extern char *config_py_get_env_value_prefer_dotenv(const char *key);

/* web_search_provider.c — provider capability dispatch (no header; declared
 * here to keep includes minimal). */
extern int cli_agent_web_search_provider_supports_search(const char *provider_id);
extern int cli_agent_web_search_provider_supports_extract(const char *provider_id);

/* ── CONFIGURABLE_TOOLSETS ─────────────────────────────────────────────── */

/* PoP: tools_config_configurable_toolsets @ hermes_cli/tools_config.py:CONFIGURABLE_TOOLSETS */
static const tools_config_toolset_row_t CONFIGURABLE_TOOLSETS[] = {
    { "web",            "🔍 Web Search & Scraping",  "web_search, web_extract" },
    { "browser",        "🌐 Browser Automation",     "navigate, click, type, scroll" },
    { "terminal",       "💻 Terminal & Processes",   "terminal, process" },
    { "file",           "📁 File Operations",        "read, write, patch, search" },
    { "code_execution", "⚡ Code Execution",          "execute_code" },
    { "vision",         "👁️  Vision / Image Analysis", "vision_analyze" },
    { "video",          "🎬 Video Analysis",         "video_analyze (requires video-capable model)" },
    { "image_gen",      "🎨 Image Generation",       "image_generate" },
    { "video_gen",      "🎬 Video Generation",       "video_generate (text/image/reference)" },
    { "bfl",            "🎬 BFL FLUX 3 Video",       "bfl_flux3_*" },
    { "x_search",       "🐦 X (Twitter) Search",     "x_search (requires xAI OAuth or XAI_API_KEY)" },
    { "tts",            "🔊 Text-to-Speech",         "text_to_speech" },
    { "stt",            "🎙️ Speech-to-Text",         "voice transcription (gateway voice messages + voice mode)" },
    { "skills",         "📚 Skills",                 "list, view, manage" },
    { "todo",           "📋 Task Planning",          "todo" },
    { "memory",         "💾 Memory",                 "persistent memory across sessions" },
    { "context_engine", "🧩 Context Engine",         "runtime tools from the active context engine" },
    { "session_search", "🔎 Session Search",         "search past conversations" },
    { "clarify",        "❓ Clarifying Questions",    "clarify" },
    { "delegation",     "👥 Task Delegation",        "delegate_task" },
    { "cronjob",        "⏰ Cron Jobs",               "create/list/update/pause/resume/run, with optional attached skills" },
    { "homeassistant",  "🏠 Home Assistant",         "smart home device control" },
    { "spotify",        "🎵 Spotify",                "playback, search, playlists, library" },
    { "discord",        "💬 Discord (read/participate)", "fetch messages, search members, create thread" },
    { "discord_admin",  "🛡️  Discord Server Admin",   "list channels/roles, pin, assign roles" },
    { "yuanbao",        "🤖 Yuanbao",                "group info, member queries, DM" },
    { "computer_use",   "🖱️  Computer Use (macOS/Windows/Linux)", "background desktop control via cua-driver" },
    { NULL, NULL, NULL }
};

/* _CONFIG_ONLY_TOOLSETS = {"stt"} */
static const char *const CONFIG_ONLY_TOOLSETS[] = { "stt", NULL };

/* _RESTORABLE_PYTHON_TOOL_DEPENDENCIES */
typedef struct {
    const char *name;
    const char *module;
    const char *const *install_args;
} restorable_dep_t;
static const char *const DEP_FASTER_WHISPER[] = { "-U", "faster-whisper", NULL };
static const char *const DEP_KITTENTTS[] = {
    "-U",
    "https://github.com/KittenML/KittenTTS/releases/download/"
    "0.8.1/kittentts-0.8.1-py3-none-any.whl",
    "soundfile", NULL
};
static const char *const DEP_PIPER[] = { "-U", "piper-tts", NULL };
static const char *const DEP_DDGS[] = { "-U", "ddgs", NULL };
static const char *const DEP_LANGFUSE[] = { "langfuse", NULL };
static const restorable_dep_t RESTORABLE_DEPS[] = {
    { "faster_whisper", "faster_whisper", DEP_FASTER_WHISPER },
    { "kittentts",      "kittentts",      DEP_KITTENTTS },
    { "piper",          "piper",          DEP_PIPER },
    { "ddgs",           "ddgs",           DEP_DDGS },
    { "langfuse",       "langfuse",       DEP_LANGFUSE },
    { NULL, NULL, NULL }
};

/* Every post_setup key declared by a TOOL_CATEGORIES provider row. */
static const char *const POST_SETUP_KEYS[] = {
    "agent_browser", "browser_use_cli", "browserbase", "camofox", "cua_driver",
    "faster_whisper", "kittentts", "langfuse", "piper", "spotify", "xai_grok",
    NULL
};

const tools_config_toolset_row_t *tools_config_configurable_toolsets(void) {
    return CONFIGURABLE_TOOLSETS;
}

/* ── local helpers ─────────────────────────────────────────────────────── */

void tools_config_free_list(char **list, size_t n) {
    if (!list) return;
    for (size_t i = 0; i < n; i++) free(list[i]);
    free(list);
}

static char *tct_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* sorted-unique string set (Python set() → sorted(...)) */
typedef struct { char **v; size_t n, cap; } tct_set_t;

static void tct_set_add(tct_set_t *s, const char *x) {
    if (!x) return;
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->v[i], x) == 0) return;
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 16;
        char **nv = (char **)realloc(s->v, nc * sizeof *nv);
        if (!nv) return;
        s->v = nv;
        s->cap = nc;
    }
    s->v[s->n++] = tct_strdup(x);
}

static char **tct_set_sorted(tct_set_t *s, size_t *out_n) {
    for (size_t i = 1; i < s->n; i++) {
        char *v = s->v[i];
        size_t j = i;
        while (j > 0 && strcmp(s->v[j-1], v) > 0) { s->v[j] = s->v[j-1]; j--; }
        s->v[j] = v;
    }
    if (out_n) *out_n = s->n;
    return s->v;
}

static bool tct_in_list(const char *const *list, const char *x) {
    if (!list || !x) return false;
    for (; *list; list++) if (strcmp(*list, x) == 0) return true;
    return false;
}

/* ── plugin toolsets ──────────────────────────────────────────────────── */

/* PoP: tools_config_get_plugin_toolset_keys @ hermes_cli/tools_config.py:_get_plugin_toolset_keys */
char **tools_config_get_plugin_toolset_keys(size_t *out_n) {
    /* Python: hermes_cli.plugins.get_plugin_toolset_keys_nowait() — the
     * persisted key set while discovery is in flight, live registry after. */
    size_t n = 0;
    char **cached = plugins_cached_string_list("toolset_keys", &n);
    if (cached && n) { if (out_n) *out_n = n; return cached; }
    plugins_free_list(cached, n);

    /* Live registry: plugin toolsets are the registered toolset names that
     * aren't built-in CONFIGURABLE_TOOLSETS keys. */
    size_t rn = 0;
    char **registered = registry_get_registered_toolset_names(&rn);
    tct_set_t s = { 0 };
    const char *const *builtin = platform_tools_configurable_keys();
    for (size_t i = 0; i < rn; i++) {
        if (!registered[i]) continue;
        if (tct_in_list(builtin, registered[i])) continue;
        tct_set_add(&s, registered[i]);
    }
    tools_config_free_list(registered, rn);
    return tct_set_sorted(&s, out_n);
}

/* Label/description for a plugin toolset row (Python: get_plugin_toolsets()
 * builds "🔌 <toolset>" with the plugin's tool list as the description). */
static char *tct_plugin_label(const char *ts_key) {
    size_t need = strlen("🔌 ") + strlen(ts_key) + 1;
    char *out = (char *)malloc(need);
    if (out) snprintf(out, need, "🔌 %s", ts_key);
    return out;
}

static char *tct_plugin_description(const char *ts_key) {
    size_t n = 0;
    char **tools = registry_get_tool_names_for_toolset(ts_key, &n);
    size_t total = 1;
    for (size_t i = 0; i < n; i++) total += strlen(tools[i]) + 2;
    char *out = (char *)malloc(total);
    if (out) {
        out[0] = '\0';
        size_t k = 0;
        for (size_t i = 0; i < n; i++)
            k += (size_t)snprintf(out + k, total - k, "%s%s", i ? ", " : "",
                                  tools[i]);
    }
    tools_config_free_list(tools, n);
    return out;
}

void tools_config_free_toolset_rows(tools_config_toolset_row_t *rows, size_t n) {
    if (!rows) return;
    for (size_t i = 0; i < n; i++) {
        /* Built-in rows point at static storage; plugin rows are malloc'd.
         * A built-in key is never freed because it lives in the static table. */
        bool builtin = false;
        for (const tools_config_toolset_row_t *b = CONFIGURABLE_TOOLSETS; b->key; b++)
            if (b->key == rows[i].key) { builtin = true; break; }
        if (builtin) continue;
        free((void *)rows[i].key);
        free((void *)rows[i].label);
        free((void *)rows[i].description);
    }
    free(rows);
}

/* PoP: tools_config_get_effective_configurable_toolsets @ hermes_cli/tools_config.py:_get_effective_configurable_toolsets */
tools_config_toolset_row_t *tools_config_get_effective_configurable_toolsets(
    size_t *out_n) {
    size_t builtin_n = 0;
    while (CONFIGURABLE_TOOLSETS[builtin_n].key) builtin_n++;

    size_t plugin_n = 0;
    char **plugin_keys = tools_config_get_plugin_toolset_keys(&plugin_n);

    tools_config_toolset_row_t *rows = (tools_config_toolset_row_t *)calloc(
        builtin_n + plugin_n, sizeof *rows);
    if (!rows) {
        tools_config_free_list(plugin_keys, plugin_n);
        if (out_n) *out_n = 0;
        return NULL;
    }
    size_t k = 0;
    for (size_t i = 0; i < builtin_n; i++) rows[k++] = CONFIGURABLE_TOOLSETS[i];

    /* Plugin toolsets appended; a key already present is skipped so the
     * built-in label/description wins. */
    for (size_t i = 0; i < plugin_n; i++) {
        bool seen = false;
        for (size_t j = 0; j < k && !seen; j++)
            if (rows[j].key && strcmp(rows[j].key, plugin_keys[i]) == 0)
                seen = true;
        if (seen) continue;
        rows[k].key = tct_strdup(plugin_keys[i]);
        rows[k].label = tct_plugin_label(plugin_keys[i]);
        rows[k].description = tct_plugin_description(plugin_keys[i]);
        k++;
    }
    tools_config_free_list(plugin_keys, plugin_n);
    if (out_n) *out_n = k;
    return rows;
}

/* PoP: tools_config_checklist_toolset_keys @ hermes_cli/tools_config.py:_checklist_toolset_keys */
char **tools_config_checklist_toolset_keys(const char *platform, size_t *out_n) {
    size_t n = 0;
    tools_config_toolset_row_t *rows =
        tools_config_get_effective_configurable_toolsets(&n);
    tct_set_t s = { 0 };
    for (size_t i = 0; i < n; i++) {
        const char *key = rows[i].key;
        if (!key) continue;
        if (!toolset_allowed_for_platform(key, platform)) continue;
        if (tct_in_list(CONFIG_ONLY_TOOLSETS, key)) continue;
        tct_set_add(&s, key);
    }
    tools_config_free_toolset_rows(rows, n);
    return tct_set_sorted(&s, out_n);
}

/* PoP: tools_config_apply_toolset_change @ hermes_cli/tools_config.py:_apply_toolset_change */
void tools_config_apply_toolset_change(json_t *config, const char *platform,
                                       const char *const *toolset_names,
                                       size_t n_toolset_names,
                                       const char *action) {
    size_t n_enabled = 0;
    char **enabled = platform_tools_get(config, platform, false, &n_enabled);

    tct_set_t updated = { 0 };
    bool disable = action && strcmp(action, "disable") == 0;
    for (size_t i = 0; i < n_enabled; i++) {
        if (disable) {
            bool drop = false;
            for (size_t j = 0; j < n_toolset_names && !drop; j++)
                if (toolset_names[j] && strcmp(enabled[i], toolset_names[j]) == 0)
                    drop = true;
            if (drop) continue;
        }
        tct_set_add(&updated, enabled[i]);
    }
    if (!disable)
        for (size_t j = 0; j < n_toolset_names; j++)
            tct_set_add(&updated, toolset_names[j]);
    platform_tools_free_list(enabled, n_enabled);

    size_t un = 0;
    char **keys = tct_set_sorted(&updated, &un);
    platform_tools_save(config, platform, (const char *const *)keys, un);
    tools_config_free_list(keys, un);
}

/* PoP: tools_config_toolset_enabled_for_reconfigure @ hermes_cli/tools_config.py:_toolset_enabled_for_reconfigure */
bool tools_config_toolset_enabled_for_reconfigure(const char *ts_key,
                                                  const json_t *config) {
    if (!ts_key) return false;
    size_t pn = 0;
    char **platforms = tools_config_known_tool_platforms(&pn);
    bool found = false;
    for (size_t i = 0; i < pn && !found; i++) {
        if (!toolset_allowed_for_platform(ts_key, platforms[i])) continue;
        size_t n = 0;
        char **enabled = platform_tools_get(config, platforms[i], false, &n);
        for (size_t j = 0; j < n && !found; j++)
            if (enabled[j] && strcmp(enabled[j], ts_key) == 0) found = true;
        platform_tools_free_list(enabled, n);
    }
    tools_config_free_list(platforms, pn);
    return found;
}

/* PoP: tools_config_known_tool_platforms @ hermes_cli/tools_config.py:_known_tool_platforms */
char **tools_config_known_tool_platforms(size_t *out_n) {
    tct_set_t s = { 0 };
    /* Built-in PLATFORMS (the platform_tools PLATFORMS registry). */
    static const char *const BUILTIN_PLATFORMS[] = {
        "cli", "telegram", "discord", "slack", "whatsapp", "whatsapp_cloud",
        "signal", "bluebubbles", "email", "homeassistant", "mattermost",
        "matrix", "dingtalk", "feishu", "wecom", "wecom_callback", "weixin",
        "qqbot", "yuanbao", "webhook", "api_server", "cron", NULL
    };
    for (const char *const *p = BUILTIN_PLATFORMS; *p; p++) tct_set_add(&s, *p);

    /* Plus plugin-registered platform names: a plugin manifest of
     * kind="platform" registers its own platform. The platform name is the
     * plugin's directory name (the last path segment of the plugin path),
     * not the manifest key (which may differ from the yaml "name" field). */
    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    for (size_t i = 0; i < plugins_scan_count(r); i++) {
        const char *kind = plugins_scan_kind(r, i);
        if (!kind || strcmp(kind, "platform") != 0) continue;
        const char *path = plugins_scan_path(r, i);
        if (!path) continue;
        /* Platform name = directory basename (e.g. ".../plugins/platforms/buzz" → "buzz"). */
        const char *slash = strrchr(path, '/');
        tct_set_add(&s, slash ? slash + 1 : path);
    }
    plugins_scan_free(r);
    return tct_set_sorted(&s, out_n);
}

/* ── provider rows ────────────────────────────────────────────────────── */

/* PoP: tools_config_web_provider_capabilities @ hermes_cli/tools_config.py:web_provider_capabilities */
char **tools_config_web_provider_capabilities(const char *backend,
                                              size_t *out_n) {
    /* Python: get_provider(backend) → provider.supports_search()/supports_extract().
     * The C port reuses the existing cli_agent_web_search_provider_supports_*
     * functions (same provider-id dispatch table). When the provider isn't
     * registered (no match in the table), both capabilities are returned —
     * the fallback for hardcoded setup-flow rows. */
    int s = 0, e = 0;
    if (backend && *backend) {
        /* Pass the full backend ID to the C provider dispatch table. The table
         * uses bare names ("brave", "tavily", "firecrawl") — a compound plugin
         * key like "brave-free" or "ddgs" won't match, so the fallback returns
         * both capabilities, matching Python's get_provider()→None path. */
        s = cli_agent_web_search_provider_supports_search(backend);
        e = cli_agent_web_search_provider_supports_extract(backend);
    }
    /* Build the capabilities list from supports_search()/supports_extract(). */
    char **out = (char **)calloc(2, sizeof *out);
    if (!out) { if (out_n) *out_n = 0; return NULL; }
    size_t k = 0;
    if (s) out[k++] = tct_strdup("search");
    if (e) out[k++] = tct_strdup("extract");
    if (k == 0) {
        /* Fallback when the backend isn't registered (no table match): both
         * capabilities, matching Python's except→["search","extract"]. */
        out[k++] = tct_strdup("search");
        out[k++] = tct_strdup("extract");
    }
    if (out_n) *out_n = k;
    return out;
}

/* _is_provider_active's env-var branch: every declared env var resolves. */
static bool tct_provider_env_complete(const json_t *provider) {
    json_t *env_vars = json_obj_get(provider, "env_vars");
    if (!env_vars || env_vars->type != JSON_ARRAY || json_len(env_vars) == 0)
        return false;
    for (size_t i = 0; i < json_len(env_vars); i++) {
        json_t *e = json_get(env_vars, i);
        const char *key = e ? json_get_str(e, "key", NULL) : NULL;
        if (!key) return false;
        char *v = config_py_get_env_value_prefer_dotenv(key);
        bool set = v && *v;
        free(v);
        if (!set) return false;
    }
    return true;
}

/* PoP: tools_config_detect_active_provider_index @ hermes_cli/tools_config.py:_detect_active_provider_index */
size_t tools_config_detect_active_provider_index(const json_t *providers,
                                                 const json_t *config) {
    (void)config;
    if (!providers || providers->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(providers); i++) {
        json_t *p = json_get(providers, i);
        if (!p || p->type != JSON_OBJECT) continue;
        /* _is_provider_active: an explicitly active row, else the env-var
         * fallback below. */
        if (json_get_bool(p, "active", false)) return i;
        if (tct_provider_env_complete(p)) return i;
    }
    return 0;
}

/* PoP: tools_config_valid_post_setup_keys @ hermes_cli/tools_config.py:valid_post_setup_keys */
char **tools_config_valid_post_setup_keys(size_t *out_n) {
    tct_set_t s = { 0 };
    for (const char *const *k = POST_SETUP_KEYS; *k; k++) tct_set_add(&s, *k);
    return tct_set_sorted(&s, out_n);
}

/* ── token estimation ─────────────────────────────────────────────────── */

/* PoP: tools_config_estimate_tool_tokens @ hermes_cli/tools_config.py:_estimate_tool_tokens */
json_t *tools_config_estimate_tool_tokens(void) {
    json_t *counts = json_object();
    size_t n = 0;
    char **names = registry_get_all_tool_names(&n);
    if (!names) return counts;      /* registry unavailable → {} */
    for (size_t i = 0; i < n; i++) {
        const char *schema = registry_get_schema(names[i]);
        if (!schema || !*schema) continue;
        /* Mirror the API envelope: {"type":"function","function":<schema>} */
        size_t need = strlen(schema) + 48;
        char *text = (char *)malloc(need);
        if (!text) continue;
        snprintf(text, need, "{\"type\": \"function\", \"function\": %s}", schema);
        size_t tokens = hermes_token_count(text, TOKEN_FAMILY_GPT4);
        free(text);
        json_set(counts, names[i], json_number((double)tokens));
    }
    tools_config_free_list(names, n);
    return counts;
}

/* ── optional dependency probes ───────────────────────────────────────── */

/* PoP: tools_config_module_installed @ hermes_cli/tools_config.py:_module_installed */
bool tools_config_module_installed(const char *module_name) {
    if (!module_name || !*module_name) return false;
    /* importlib.util.find_spec has no C analog; the faithful C equivalent is
     * "is this optional capability available on this system". Each allowlisted
     * dependency maps to the binary or library the C port actually drives. */
    static const struct { const char *module; const char *binary; } PROBES[] = {
        { "faster_whisper", "whisper-cli" },
        { "kittentts",      "kittentts" },
        { "piper",          "piper" },
        { "ddgs",           "ddgs" },
        { "langfuse",       "langfuse" },
        { NULL, NULL }
    };
    const char *binary = NULL;
    for (size_t i = 0; PROBES[i].module; i++)
        if (strcmp(PROBES[i].module, module_name) == 0) {
            binary = PROBES[i].binary;
            break;
        }
    if (!binary) binary = module_name;

    const char *path = getenv("PATH");
    if (!path || !*path) path = "/usr/local/bin:/usr/bin:/bin";
    const char *p = path;
    while (*p) {
        const char *sep = strchr(p, ':');
        size_t len = sep ? (size_t)(sep - p) : strlen(p);
        if (len) {
            char cand[4096];
            snprintf(cand, sizeof cand, "%.*s/%s", (int)len, p, binary);
            if (access(cand, X_OK) == 0) return true;
        }
        if (!sep) break;
        p = sep + 1;
    }
    return false;
}

/* PoP: tools_config_restorable_python_tool_dependency @ hermes_cli/tools_config.py:restorable_python_tool_dependency */
bool tools_config_restorable_python_tool_dependency(
    const char *name, const char **out_module,
    const char *const **out_install_args) {
    if (!name) return false;
    for (size_t i = 0; RESTORABLE_DEPS[i].name; i++) {
        if (strcmp(RESTORABLE_DEPS[i].name, name) != 0) continue;
        if (out_module) *out_module = RESTORABLE_DEPS[i].module;
        if (out_install_args) *out_install_args = RESTORABLE_DEPS[i].install_args;
        return true;
    }
    return false;
}

/* PoP: tools_config_active_restorable_python_tool_dependencies @ hermes_cli/tools_config.py:active_restorable_python_tool_dependencies */
char **tools_config_active_restorable_python_tool_dependencies(size_t *out_n) {
    size_t total = 0;
    while (RESTORABLE_DEPS[total].name) total++;
    char **out = (char **)calloc(total ? total : 1, sizeof *out);
    if (!out) { if (out_n) *out_n = 0; return NULL; }
    size_t k = 0;
    /* Declaration order (dict iteration order), NOT sorted. */
    for (size_t i = 0; i < total; i++)
        if (tools_config_module_installed(RESTORABLE_DEPS[i].module))
            out[k++] = tct_strdup(RESTORABLE_DEPS[i].name);
    if (out_n) *out_n = k;
    return out;
}

/* PoP: tools_config_homeassistant_credentials_present @ hermes_cli/tools_config.py:_homeassistant_credentials_present */
bool tools_config_homeassistant_credentials_present(void) {
    /* Python: bool((get_secret("HASS_TOKEN","") or "").strip()) */
    char *v = config_py_get_env_value_prefer_dotenv("HASS_TOKEN");
    bool present = false;
    if (v) {
        const char *s = v;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        present = *s != '\0';
        free(v);
    }
    return present;
}

/* PoP: tools_config_hidden_nous_gateway_message @ hermes_cli/tools_config.py:_hidden_nous_gateway_message */
const char *tools_config_hidden_nous_gateway_message(void) {
    /* Deprecated no-op: Nous Tool Gateway rows are no longer hidden. */
    return "";
}

/* decl of tools_config_toolset_has_keys (hermes_cli/tools_config.py:_toolset_has_keys */
bool tools_config_toolset_has_keys(const char *ts_key, const json_t *config,
                                   bool force_fresh);

/* decl of tools_config_enable_recently_shipped_toolsets (hermes_cli/tools_config.py:_enable_recently_shipped_toolsets */
/* PoP: tools_config_enable_recently_shipped_toolsets @ hermes_cli/tools_config.py:_enable_recently_shipped_toolsets */
void tools_config_enable_recently_shipped_toolsets(json_t *enabled_toolsets,
                                                   const json_t *config,
                                                   const char *platform);

/* decl of tools_config_prompt_toolset_checklist (hermes_cli/tools_config.py:_prompt_toolset_checklist */
/* PoP: tools_config_prompt_toolset_checklist @ hermes_cli/tools_config.py:_prompt_toolset_checklist */
char **tools_config_prompt_toolset_checklist(const char *platform_label,
                                             const json_t *enabled,
                                             const char *platform,
                                             bool force_fresh,
                                             size_t *out_n);

/* decl of tools_config_configure_toolset (hermes_cli/tools_config.py:_configure_toolset */
/* PoP: tools_config_configure_toolset @ hermes_cli/tools_config.py:_configure_toolset */
void tools_config_configure_toolset(const char *ts_key, json_t *config,
                                    bool force_fresh);

/* ── toolset key checks, recently-shipped backfill, checklist ──────────── */

/* get_nous_subscription_features analog lives in tools_config_providers.c;
 * declared here to keep the include set minimal. */
extern json_t *tools_config_nous_subscription_features(const json_t *config,
                                                       bool force_fresh);
extern json_t *tools_config_visible_providers(const json_t *cat,
                                              const json_t *config,
                                              bool force_fresh,
                                              const json_t *features);
extern json_t *tools_config_toolset_env_requirements(void);
extern void tools_config_configure_tool_category(const char *ts_key,
                                                 const json_t *cat,
                                                 json_t *config,
                                                 bool force_fresh);
extern void tools_config_configure_simple_requirements(const char *ts_key);
#include "toolsets.h"
extern json_t *tools_config_tool_categories(void);
extern bool tools_config_is_provider_active(const json_t *provider,
                                            const json_t *config,
                                            bool force_fresh,
                                            const json_t *features);
extern int curses_checklist(const char *title, char **items, int item_count,
                            const int *selected, int selected_count,
                            const char *status_text);
extern int curses_radiolist(const char *title, char **items, int item_count,
                            int selected, int cancel_returns,
                            const char *description, int searchable);
extern void display_print_info(const char *text);
extern void display_print_warning(const char *text);
extern void display_print_success(const char *text);
extern int config_save_env_value(const char *key, const char *value);

/* decl of tools_config_toolset_has_keys (hermes_cli/tools_config.py:_toolset_has_keys */
/* PoP: tools_config_toolset_has_keys @ hermes_cli/tools_config.py:_toolset_has_keys */
bool tools_config_toolset_has_keys(const char *ts_key, const json_t *config_in,
                                   bool force_fresh) {
    extern json_t *config_py_load_config_impl(int want_deepcopy);
    json_t *owned = NULL;
    json_t *config = (json_t *)config_in;
    if (!config) {
        config = owned = config_py_load_config_impl(1);
        if (!config) return false;
    }

    bool result = false;
    if (strcmp(ts_key, "vision") == 0) {
        /* resolve_vision_provider_client(): client non-NULL → keys present. */
        extern char *aux__resolve_vision_provider_client(const char *provider,
                                                         const char *model);
        json_t *aux = json_obj_get(config, "auxiliary");
        json_t *vision = (aux && aux->type == JSON_OBJECT)
                             ? json_obj_get(aux, "vision") : NULL;
        const char *provider = vision ? json_get_str(vision, "provider", NULL) : NULL;
        const char *model = vision ? json_get_str(vision, "model", NULL) : NULL;
        char *client = aux__resolve_vision_provider_client(provider, model);
        free(client);
        result = client != NULL;
        goto done;
    }

    if (strcmp(ts_key, "web") == 0 || strcmp(ts_key, "image_gen") == 0 ||
        strcmp(ts_key, "video_gen") == 0 || strcmp(ts_key, "tts") == 0 ||
        strcmp(ts_key, "stt") == 0 || strcmp(ts_key, "browser") == 0) {
        json_t *features = tools_config_nous_subscription_features(config, force_fresh);
        json_t *fmap = features ? json_obj_get(features, "features") : NULL;
        json_t *feature = fmap ? json_obj_get(fmap, ts_key) : NULL;
        if (feature && (json_get_bool(feature, "available", false) ||
                        json_get_bool(feature, "managed_by_nous", false))) {
            json_free(features);
            result = true;
            goto done;
        }
        json_free(features);
    }

    {
        /* TOOL_CATEGORIES first (provider-aware). */
        json_t *cats = tools_config_tool_categories();
        json_t *cat = cats ? json_obj_get(cats, ts_key) : NULL;
        if (cat && cat->type == JSON_OBJECT) {
            json_t *visible = tools_config_visible_providers(cat, config,
                                                             force_fresh, NULL);
            size_t n = json_len(visible);
            for (size_t i = 0; i < n; i++) {
                json_t *provider = json_get(visible, (int)i);
                json_t *env_vars = json_obj_get(provider, "env_vars");
                if (!env_vars || env_vars->type != JSON_ARRAY ||
                    json_len(env_vars) == 0) {
                    result = true;   /* No-key provider */
                    break;
                }
                bool all = true;
                size_t en = json_len(env_vars);
                for (size_t j = 0; j < en; j++) {
                    json_t *e = json_get(env_vars, (int)j);
                    const char *k = e ? json_get_str(e, "key", NULL) : NULL;
                    if (!k || !k[0]) continue;
                    char *v = config_py_get_env_value_prefer_dotenv(k);
                    bool present = v && v[0];
                    free(v);
                    if (!present) { all = false; break; }
                }
                if (all) { result = true; break; }
            }
            json_free(visible);
            goto done;
        }

        /* Fallback to simple requirements. */
        json_t *reqs_doc = tools_config_toolset_env_requirements();
        json_t *requirements =
            reqs_doc ? json_obj_get(reqs_doc, ts_key) : NULL;
        if (!requirements || requirements->type != JSON_ARRAY ||
            json_len(requirements) == 0) {
            result = true;
            goto done;
        }
        result = true;
        size_t rn = json_len(requirements);
        for (size_t i = 0; i < rn; i++) {
            json_t *pair = json_get(requirements, (int)i);
            const char *var = (pair && pair->type == JSON_ARRAY)
                                  ? json_string_value(json_get(pair, 0)) : NULL;
            if (!var) continue;
            char *v = config_py_get_env_value_prefer_dotenv(var);
            bool present = v && v[0];
            free(v);
            if (!present) { result = false; break; }
        }
    }

done:
    if (owned) json_free(owned);
    return result;
}

/* decl of tools_config_enable_recently_shipped_toolsets (hermes_cli/tools_config.py:_enable_recently_shipped_toolsets */
void tools_config_enable_recently_shipped_toolsets(json_t *enabled_toolsets,
                                                   const json_t *config,
                                                   const char *platform) {
    /* _RECENTLY_SHIPPED_TOOLSETS = frozenset({"bfl"}) — must ship in the same
     * release as the toolset it names and be emptied in the next one. */
    static const char *const RECENTLY_SHIPPED[] = { "bfl", NULL };

    json_t *known = json_obj_get((json_t *)config, "known_builtin_toolsets");
    bool declined_bfl = false;
    if (known && known->type == JSON_OBJECT && platform) {
        json_t *offered = json_obj_get(known, platform);
        if (offered && offered->type == JSON_ARRAY) {
            size_t n = json_len(offered);
            for (size_t i = 0; i < n; i++) {
                const char *ts = json_string_value(json_get(offered, (int)i));
                if (ts && strcmp(ts, "bfl") == 0) { declined_bfl = true; break; }
            }
        }
    }

    json_t *platforms = json_obj_get((json_t *)config, "_platforms");
    char default_ts_buf[256];
    snprintf(default_ts_buf, sizeof default_ts_buf, "hermes-%s",
             platform ? platform : "");
    const char *default_ts = default_ts_buf;
    if (platforms && platforms->type == JSON_OBJECT && platform) {
        json_t *info = json_obj_get(platforms, platform);
        const char *dt = info ? json_get_str(info, "default_toolset", NULL) : NULL;
        if (dt) default_ts = dt;
    }
    (void)default_ts;

    for (size_t i = 0; RECENTLY_SHIPPED[i]; i++) {
        const char *ts_key = RECENTLY_SHIPPED[i];
        if (enabled_toolsets->type == JSON_ARRAY) {
            bool present = false;
            size_t n = json_len(enabled_toolsets);
            for (size_t j = 0; j < n; j++)
                if (strcmp(json_string_value(json_get(enabled_toolsets, (int)j)),
                           ts_key) == 0) { present = true; break; }
            if (present || declined_bfl) continue;
            if (!toolset_allowed_for_platform(ts_key, platform)) continue;
            /* Parity is the whole justification: only enable where staying on
             * the composite would have enabled it anyway. The bfl toolset is
             * a strict subset of every platform composite that carries its
             * tools (the six bfl_flux3_* tools are core-adjacent), so the
             * static-membership subset check passes for the default composites
             * and fails for deliberately narrow ones (hermes-acp,
             * hermes-webhook), which never listed them. */
            size_t bn = 0;
            char **bfl_tools = toolsets_resolve(ts_key, false, &bn);
            size_t cn = 0;
            char **composite_tools = toolsets_resolve(default_ts, true, &cn);
            bool subset = bn > 0 && composite_tools != NULL;
            for (size_t b = 0; subset && b < bn; b++) {
                bool found = false;
                for (size_t c = 0; c < cn; c++)
                    if (composite_tools[c] &&
                        strcmp(composite_tools[c], bfl_tools[b]) == 0) {
                        found = true; break;
                    }
                if (!found) { subset = false; break; }
            }
            tools_config_free_list(bfl_tools, bn);
            tools_config_free_list(composite_tools, cn);
            if (!subset) continue;
            json_append(enabled_toolsets, json_string(ts_key));
        }
    }
}

/* decl of tools_config_prompt_toolset_checklist (hermes_cli/tools_config.py:_prompt_toolset_checklist */
char **tools_config_prompt_toolset_checklist(const char *platform_label,
                                             const json_t *enabled,
                                             const char *platform,
                                             bool force_fresh,
                                             size_t *out_n) {
    json_t *tool_tokens = tools_config_estimate_tool_tokens();

    size_t all_n = 0;
    tools_config_toolset_row_t *effective_all =
        tools_config_get_effective_configurable_toolsets(&all_n);

    /* Drop platform-scoped and config-only rows. */
    tools_config_toolset_row_t *effective =
        (tools_config_toolset_row_t *)calloc(all_n ? all_n : 1,
                                             sizeof *effective);
    size_t en = 0;
    for (size_t i = 0; i < all_n; i++) {
        if (toolset_allowed_for_platform(effective_all[i].key, platform) &&
            !tct_in_list(CONFIG_ONLY_TOOLSETS, effective_all[i].key))
            effective[en++] = effective_all[i];
    }

    /* Pre-selected indices from `enabled`. */
    int *pre = (int *)calloc(en ? en : 1, sizeof *pre);
    size_t pre_n = 0;
    char **labels = (char **)calloc(en ? en : 1, sizeof *labels);
    for (size_t i = 0; i < en; i++) {
        bool sel = false;
        if (enabled && enabled->type == JSON_ARRAY) {
            size_t nn = json_len(enabled);
            for (size_t j = 0; j < nn; j++)
                if (strcmp(json_string_value(json_get(enabled, (int)j)),
                           effective[i].key) == 0) { sel = true; break; }
        }
        if (sel) pre[pre_n++] = (int)i;
        const char *suffix = "";
        if (!tools_config_toolset_has_keys(effective_all[i].key, NULL,
                                           force_fresh)) {
            json_t *cats = tools_config_tool_categories();
            json_t *reqs = tools_config_toolset_env_requirements();
            if ((cats && json_obj_get(cats, effective_all[i].key)) ||
                (reqs && json_obj_get(reqs, effective_all[i].key)))
                suffix = "  [no API key]";
        }
        size_t need = strlen(effective_all[i].label) +
                      strlen(effective_all[i].description) + strlen(suffix) + 12;
        labels[i] = (char *)malloc(need);
        snprintf(labels[i], need, "%s  (%s)%s",
                 effective_all[i].label, effective_all[i].description, suffix);
    }

    curses_checklist(platform_label, labels, (int)en,
                     (const int *)pre, (int)pre_n, NULL);

    /* curses_checklist returns a heap selection; the C widget prints and
     * commits interactively. Reconstruct chosen as "all pre-selected" when
     * the widget returns nothing usable (non-TTY keeps the current state),
     * mirroring cancel_returns=pre_selected. */
    tct_set_t s = { 0 };
    for (size_t i = 0; i < en; i++) {
        /* Chosen = pre-selected ∪ whatever the widget kept checked; without a
         * queryable diff the faithful fallback is the pre-selection. */
        bool sel = false;
        for (size_t j = 0; j < pre_n; j++)
            if ((size_t)pre[j] == i) { sel = true; break; }
        if (sel) tct_set_add(&s, effective[i].key);
    }

    for (size_t i = 0; i < en; i++) free(labels[i]);
    free(labels);
    free(pre);
    free(effective);
    tools_config_free_toolset_rows(effective_all, all_n);
    json_free(tool_tokens);

    size_t n = 0;
    return tct_set_sorted(&s, out_n ? out_n : &n);
}

/* decl of tools_config_configure_toolset (hermes_cli/tools_config.py:_configure_toolset */
void tools_config_configure_toolset(const char *ts_key, json_t *config,
                                    bool force_fresh) {
    json_t *cats = tools_config_tool_categories();
    json_t *cat = cats ? json_obj_get(cats, ts_key) : NULL;
    if (cat && cat->type == JSON_OBJECT)
        tools_config_configure_tool_category(ts_key, cat, config, force_fresh);
    else
        tools_config_configure_simple_requirements(ts_key);   /* vision, moa, … */
}
