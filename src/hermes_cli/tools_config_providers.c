/* tools_config_providers.c — faithful C11 port of the provider-aware
 * configuration surface of hermes_cli/tools_config.py.
 *
 * Reuses: hermes_json (libjson), tools_config_cua.h, tools_config_install.h,
 * tools/browser_use_cli + browser_tool probes, curses_ui radiolist/checklist
 * ports, image_gen/video_gen/web_search/browser/tts registry ports,
 * config env helpers.
 */
#define _GNU_SOURCE
#include "tools_config_providers.h"
#include "tools_config_cua.h"
#include "tools_config_install.h"

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hermes_display.h"
#include "port_config_py_helpers.h"
#include "image_gen_registry.h"
#include "video_gen_registry.h"
#include "web_search_registry.h"
#include "browser_registry.h"
#include "tools_config_toolsets.h"

/* ── externs from sibling ports (headers kept minimal) ─────────────────── */
extern int curses_radiolist(const char *title, char **items, int item_count,
                            int selected, int cancel_returns,
                            const char *description, int searchable);
extern char *stp_prompt(const char *question, const char *default_value);
extern int config_save_env_value(const char *key, const char *value);
extern bool plug_ensure_discovered(void);
/* registries under src/agent/ */
extern bool browser_chromium_installed(void);
extern bool browser_running_in_docker(void);
extern char *browser_find_agent_browser(void);
extern int bu_find_cli(char *out_path, size_t out_sz);

/* NOUS_MANAGED_PROVIDER = "nous" */
static const char NOUS_MANAGED_PROVIDER[] = "nous";

/* ── small helpers ─────────────────────────────────────────────────────── */

static char *tcp_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static bool tcp_truthy(const json_t *v, bool def) {
    /* is_truthy_value(v, default): false/0/no/off strings are falsey. */
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        if (!s[0]) return def;   /* empty string → not a recognized literal */
        if (strcasecmp(s, "false") == 0 || strcmp(s, "0") == 0 ||
            strcasecmp(s, "no") == 0 || strcasecmp(s, "off") == 0)
            return false;
        if (strcasecmp(s, "true") == 0 || strcmp(s, "1") == 0 ||
            strcasecmp(s, "yes") == 0 || strcasecmp(s, "on") == 0)
            return true;
        return def;
    }
    return def;
}

/* provider.get(key) as string; NULL when absent. */
static const char *tcp_prov_str(const json_t *provider, const char *key) {
    static char bufs[8][512];
    static int rot = 0;
    json_t *v = json_obj_get((json_t *)provider, key);
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) {
        char *buf = bufs[rot++ & 7];
        snprintf(buf, 512, "%s", v->str_val);
        return buf;
    }
    return NULL;
}

static bool tcp_has_key(const json_t *obj, const char *key) {
    return obj && json_obj_get((json_t *)obj, key) != NULL;
}

static const char *tcp_str_or(const json_t *obj, const char *key,
                              const char *def) {
    json_t *v = json_obj_get((json_t *)obj, key);
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0])
        return v->str_val;
    return def;
}

static bool tcp_bool_or(const json_t *obj, const char *key, bool def) {
    json_t *v = json_obj_get((json_t *)obj, key);
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0;
    if (v->type == JSON_STRING) return tcp_truthy(v, def);
    return def;
}

/* cfg_get(config, section, key) as string; NULL when unset. */
static const char *tcp_cfg_get(const json_t *config, const char *section,
                               const char *key) {
    if (!config || config->type != JSON_OBJECT) return NULL;
    json_t *sec = json_obj_get((json_t *)config, section);
    if (!sec || sec->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(sec, key);
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return NULL;
}

/* get_env_value analog: .env-preferred lookup. */
static char *tcp_get_env(const char *key) {
    return config_py_get_env_value_prefer_dotenv(key);
}

static bool tcp_env_present(const char *key) {
    char *v = tcp_get_env(key);
    bool present = v && v[0];
    free(v);
    return present;
}

/* All env_vars entries' keys set? Empty array → true (vacuous). */
static bool tcp_all_env_vars_set(const json_t *provider) {
    json_t *env_vars = json_obj_get((json_t *)provider, "env_vars");
    if (!env_vars || env_vars->type != JSON_ARRAY) return true;
    size_t n = json_len(env_vars);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(env_vars, (int)i);
        const char *k = e ? json_get_str(e, "key", NULL) : NULL;
        if (!k || !tcp_env_present(k)) return false;
    }
    return true;
}

/* _prompt_choice: single-select menu delegating to curses_radiolist with
 * cancel_returns=default. */
static int tcp_prompt_choice(const char *question, char **choices,
                             int n_choices, int def) {
    return curses_radiolist(question, choices, n_choices, def, def, NULL, 1);
}

/* _prompt(question, password=...) — line read with optional echo-off. */
static char *tcp_prompt_hidden(const char *question) {
    if (question) printf("%s: ", question), fflush(stdout);
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, stdin);
    if (n < 0) { free(line); return tcp_strdup(""); }
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = '\0';
    return line;
}

static void tcp_print_color_line(const char *text) {
    /* print(color(...)) analog: plain text to stdout. */
    printf("%s\n", text ? text : "");
}

/* ── module-level data registries ──────────────────────────────────────── */

static json_t *g_tool_categories = NULL;          /* borrowed */
static json_t *g_toolset_env_requirements = NULL; /* borrowed */

void tools_config_register_tool_categories(json_t *categories) {
    g_tool_categories = categories;
}
json_t *tools_config_tool_categories(void) { return g_tool_categories; }

void tools_config_register_toolset_env_requirements(json_t *reqs) {
    g_toolset_env_requirements = reqs;
}
json_t *tools_config_toolset_env_requirements(void) {
    return g_toolset_env_requirements;
}

json_t *tools_config_nous_subscription_features(const json_t *config,
                                                bool force_fresh) {
    (void)force_fresh;
    /* The live portal fetch is a network boundary the C runtime resolves at
     * login time and mirrors into the config document (keys nous_auth_present
     * / nous_account_info / nous_features). Absent → logged-out features. */
    json_t *out = json_object();
    bool auth_present = config ? tcp_bool_or(config, "nous_auth_present", false)
                               : false;
    json_set(out, "nous_auth_present", json_bool(auth_present));
    json_t *account = json_object();
    json_t *src_acct =
        config ? json_obj_get((json_t *)config, "nous_account_info") : NULL;
    if (src_acct && src_acct->type == JSON_OBJECT) {
        json_set(account, "logged_in",
                 json_bool(tcp_bool_or(src_acct, "logged_in", false)));
        json_set(account, "paid_service_access",
                 tcp_truthy(json_obj_get(src_acct, "paid_service_access"), false)
                     ? json_bool(true)
                     : json_bool(false));
        json_set(account, "tool_gateway_entitled",
                 json_bool(tcp_bool_or(src_acct, "tool_gateway_entitled", false)));
    } else {
        json_set(account, "logged_in", json_bool(false));
        json_set(account, "paid_service_access", json_bool(false));
        json_set(account, "tool_gateway_entitled", json_bool(false));
    }
    json_set(out, "account_info", account);
    json_t *features = json_object();
    json_t *src_feat =
        config ? json_obj_get((json_t *)config, "nous_features") : NULL;
    if (src_feat && src_feat->type == JSON_OBJECT) {
        size_t n = json_object_size(src_feat);
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(src_feat, i);
            json_t *v = json_object_get_at(src_feat, i);
            if (k && v) json_set(features, k, json_copy(v));
        }
    }
    json_set(out, "features", features);
    return out;
}

/* ── plugin provider catalogs ──────────────────────────────────────────── */

/* Shared row builder: schema.get(field, default) traversal over the C
 * provider surface. The C registries expose name/display_name but their
 * get_setup_schema() analogs return validation schemas without picker
 * fields, so absent schema fields fall back exactly like Python's
 * schema.get(..., provider.display_name) / [] defaults. */
static void tcp_row_common(json_t *row, const char *schema_name,
                           const char *schema_badge, const char *schema_tag,
                           const char *display_fallback) {
    json_set(row, "name", json_string(schema_name ? schema_name
                                                  : (display_fallback ? display_fallback : "")));
    json_set(row, "badge", json_string(schema_badge ? schema_badge : ""));
    json_set(row, "tag", json_string(schema_tag ? schema_tag : ""));
    /* env_vars: the C schema surface carries none → empty list default. */
    json_set(row, "env_vars", json_array());
}

static const char *const TTS_BUILTIN_NAMES[] = {
    "edge", "openai", "elevenlabs", "minimax", "xai", "mistral",
    "gemini", "neutts", "kittentts", "piper", NULL,
};

/* PoP: tools_config_plugin_image_gen_providers @ hermes_cli/tools_config.py:_plugin_image_gen_providers */
json_t *tools_config_plugin_image_gen_providers(void) {
    json_t *rows = json_array();
    if (!plug_ensure_discovered()) return rows;
    size_t n = (size_t)image_gen_provider_count();
    for (size_t i = 0; i < n; i++) {
        const image_gen_provider_t *provider = image_gen_get_provider_by_index((int)i);
        if (!provider) continue;   /* get_setup_schema() exception → continue */
        json_t *row = json_object();
        /* Schema carries no picker fields in the C port — fall back to
         * display_name / "" / [] exactly like the .get() defaults. */
        tcp_row_common(row, NULL, NULL, NULL, provider->display_name);
        json_set(row, "image_gen_plugin_name", json_string(provider->name));
        json_append(rows, row);
    }
    return rows;
}

/* PoP: tools_config_plugin_video_gen_providers @ hermes_cli/tools_config.py:_plugin_video_gen_providers */
json_t *tools_config_plugin_video_gen_providers(void) {
    json_t *rows = json_array();
    if (!plug_ensure_discovered()) return rows;
    size_t n = (size_t)video_gen_provider_count();
    for (size_t i = 0; i < n; i++) {
        const video_gen_provider_t *provider =
            video_gen_get_provider_by_index((int)i);
        if (!provider) continue;
        json_t *row = json_object();
        tcp_row_common(row, NULL, NULL, NULL, provider->display_name);
        json_set(row, "video_gen_plugin_name", json_string(provider->name));
        json_append(rows, row);
    }
    return rows;
}

/* PoP: tools_config_plugin_web_search_providers @ hermes_cli/tools_config.py:_plugin_web_search_providers */
json_t *tools_config_plugin_web_search_providers(void) {
    json_t *rows = json_array();
    if (!plug_ensure_discovered()) return rows;
    size_t n = (size_t)web_search_provider_count();
    for (size_t i = 0; i < n; i++) {
        const web_search_provider_t *provider =
            web_search_get_provider_by_index((int)i);
        if (!provider || !provider->name[0]) continue;   /* no name → skip */
        /* Tier variants flatten here when a schema exposes them; the C
         * registry surface has no variants field → base row only. */
        json_t *row = json_object();
        tcp_row_common(row, NULL, NULL, NULL, provider->display_name);
        json_set(row, "web_backend", json_string(provider->name));
        json_set(row, "web_search_plugin_name", json_string(provider->name));
        json_append(rows, row);
    }
    return rows;
}

/* PoP: tools_config_plugin_browser_providers @ hermes_cli/tools_config.py:_plugin_browser_providers */
json_t *tools_config_plugin_browser_providers(void) {
    json_t *rows = json_array();
    if (!plug_ensure_discovered()) return rows;
    const browser_provider_t **list = NULL;
    int n = browser_registry_list(&list);
    for (int i = 0; i < n; i++) {
        const browser_provider_t *provider = list[i];
        if (!provider || !provider->name || !provider->name[0]) continue;
        json_t *row = json_object();
        const char *display = provider->display_name ? provider->display_name
                                                     : provider->name;
        tcp_row_common(row, NULL, NULL, NULL, display);
        json_set(row, "browser_provider", json_string(provider->name));
        json_set(row, "browser_plugin_name", json_string(provider->name));
        json_append(rows, row);
    }
    free(list);
    return rows;
}

/* PoP: tools_config_plugin_tts_providers @ hermes_cli/tools_config.py:_plugin_tts_providers */
json_t *tools_config_plugin_tts_providers(void) {
    json_t *rows = json_array();
    if (!plug_ensure_discovered()) return rows;
    extern char *tts_list_providers(void);
    char *joined = tts_list_providers();
    if (!joined) return rows;
    /* comma-separated registered names; filter built-in shadowing. */
    char *saveptr = NULL;
    for (char *tok = strtok_r(joined, ",", &saveptr); tok;
         tok = strtok_r(NULL, ",", &saveptr)) {
        while (*tok == ' ') tok++;
        if (!*tok) continue;
        bool builtin = false;
        for (size_t b = 0; TTS_BUILTIN_NAMES[b]; b++) {
            if (strcasecmp(tok, TTS_BUILTIN_NAMES[b]) == 0) { builtin = true; break; }
        }
        if (builtin) continue;   /* defensive: reject built-in shadowing */
        json_t *row = json_object();
        tcp_row_common(row, tok, NULL, NULL, tok);
        json_set(row, "tts_provider", json_string(tok));
        json_set(row, "tts_plugin_name", json_string(tok));
        json_append(rows, row);
    }
    free(joined);
    return rows;
}

/* PoP: tools_config_visible_providers @ hermes_cli/tools_config.py:_visible_providers */
json_t *tools_config_visible_providers(const json_t *cat, const json_t *config,
                                       bool force_fresh,
                                       const json_t *features) {
    json_t *visible = json_array();
    json_t *feat = features ? json_copy((json_t *)features)
                            : tools_config_nous_subscription_features(config, force_fresh);
    json_t *acct = feat ? json_obj_get(feat, "account_info") : NULL;
    bool logged_in = acct ? tcp_bool_or(acct, "logged_in", false) : false;
    bool paid = acct && tcp_bool_or(acct, "paid_service_access", false);
    bool entitled = acct ? tcp_bool_or(acct, "tool_gateway_entitled", false) : false;
    /* Pool-only users get image gen but NOT video gen. */
    bool pool_only = logged_in && !paid && entitled;
    bool auth_present = feat ? tcp_bool_or(feat, "nous_auth_present", false)
                             : false;

    json_t *providers = cat ? json_obj_get((json_t *)cat, "providers") : NULL;
    if (providers && providers->type == JSON_ARRAY) {
        size_t n = json_len(providers);
        for (size_t i = 0; i < n; i++) {
            json_t *provider = json_get(providers, (int)i);
            if (!provider) continue;
            bool requires_auth = tcp_bool_or(provider, "requires_nous_auth", false);
            const char *managed = tcp_prov_str(provider, "managed_nous_feature");
            if (requires_auth && !managed && !auth_present)
                continue;
            if (pool_only && managed && strcmp(managed, "video_gen") == 0 &&
                !(acct && tcp_bool_or(acct, "tool_gateway_entitled_for_fal_video", false)))
                continue;
            json_append(visible, json_copy(provider));
        }
    }

    const char *cat_name = cat ? tcp_str_or(cat, "name", "") : "";
    json_t *extra = NULL;
    if (strcmp(cat_name, "Image Generation") == 0)
        extra = tools_config_plugin_image_gen_providers();
    else if (strcmp(cat_name, "Video Generation") == 0)
        extra = tools_config_plugin_video_gen_providers();
    else if (strcmp(cat_name, "Web Search & Extract") == 0)
        extra = tools_config_plugin_web_search_providers();
    else if (strcmp(cat_name, "Browser Automation") == 0)
        extra = tools_config_plugin_browser_providers();
    else if (strcmp(cat_name, "Text-to-Speech") == 0)
        extra = tools_config_plugin_tts_providers();
    if (extra) {
        size_t en = json_len(extra);
        for (size_t i = 0; i < en; i++)
            json_append(visible, json_copy(json_get(extra, (int)i)));
        json_free(extra);
    }

    json_free(feat);
    return visible;
}

/* ── installed-state probes ────────────────────────────────────────────── */

/* PoP: tools_config_agent_browser_installed @ hermes_cli/tools_config.py:_agent_browser_installed */
bool tools_config_agent_browser_installed(void) {
    /* The C chromium probe is uncached (the Python cache-drop is moot). */
    char path[1024];
    bool cli_ok = bu_find_cli(path, sizeof path) == 1;
    if (!cli_ok) return false;
    /* Lightpanda needs no Chromium; otherwise require the browser build. */
    return browser_chromium_installed() || access("/usr/bin/lightpanda", X_OK) == 0;
}

/* PoP: tools_config_camofox_installed @ hermes_cli/tools_config.py:_camofox_installed */
bool tools_config_camofox_installed(void) {
    const char *root = getenv("HERMES_PROJECT_ROOT");
    root = (root && *root) ? root : ".";
    char path[4096];
    snprintf(path, sizeof path, "%s/node_modules/@askjo/camofox-browser", root);
    struct stat st;
    return stat(path, &st) == 0;
}

/* PoP: tools_config_cloud_agent_browser_installed @ hermes_cli/tools_config.py:_cloud_agent_browser_installed */
bool tools_config_cloud_agent_browser_installed(void) {
    /* Cloud providers host their own Chromium — CLI presence is everything. */
    char path[1024];
    return bu_find_cli(path, sizeof path) == 1;
}

/* ── provider activity ─────────────────────────────────────────────────── */

/* PoP: tools_config_web_tier_matches @ hermes_cli/tools_config.py:_web_tier_matches */
bool tools_config_web_tier_matches(const json_t *provider, const json_t *config) {
    const char *row_tier = tcp_prov_str(provider, "web_tier");
    if (!row_tier || !*row_tier)
        return true;   /* tier-agnostic row */
    json_t *web_cfg = config ? json_obj_get((json_t *)config, "web") : NULL;
    json_t *tiers = (web_cfg && web_cfg->type == JSON_OBJECT)
                        ? json_obj_get(web_cfg, "provider_tier") : NULL;
    const char *backend = tcp_prov_str(provider, "web_backend");
    char configured[128] = "";
    if (tiers && tiers->type == JSON_OBJECT && backend) {
        json_t *t = json_obj_get(tiers, backend);
        if (t && t->type == JSON_STRING && t->str_val) {
            /* str(...).lower().strip() */
            const char *s = t->str_val;
            while (*s == ' ' || *s == '\t') s++;
            snprintf(configured, sizeof configured, "%s", s);
            size_t l = strlen(configured);
            while (l && (configured[l-1] == ' ' || configured[l-1] == '\t'))
                configured[--l] = '\0';
            for (char *p = configured; *p; p++)
                if (*p >= 'A' && *p <= 'Z') *p += 32;
        }
    }
    if (strcmp(configured, "free") == 0 || strcmp(configured, "paid") == 0)
        return strcmp(configured, row_tier) == 0;
    /* Auto: key present → paid. */
    bool has_key = false;
    if (backend && strcmp(backend, "exa") == 0)
        has_key = tcp_env_present("EXA_API_KEY");
    else if (backend && strcmp(backend, "parallel") == 0)
        has_key = tcp_env_present("PARALLEL_API_KEY");
    return strcmp(row_tier, has_key ? "paid" : "free") == 0;
}

/* PoP: tools_config_is_provider_active @ hermes_cli/tools_config.py:_is_provider_active */
bool tools_config_is_provider_active(const json_t *provider,
                                     const json_t *config, bool force_fresh,
                                     const json_t *features) {
    const char *plugin_name = tcp_prov_str(provider, "image_gen_plugin_name");
    const char *managed = tcp_prov_str(provider, "managed_nous_feature");

    if (plugin_name && !managed) {
        json_t *img = config ? json_obj_get((json_t *)config, "image_gen") : NULL;
        if (!(img && img->type == JSON_OBJECT)) return false;
        const char *cur = tcp_str_or(img, "provider", "");
        if (!cur || strcmp(cur, plugin_name) != 0) return false;
        return !tcp_truthy(json_obj_get(img, "use_gateway"), false);
    }

    const char *video_plugin = tcp_prov_str(provider, "video_gen_plugin_name");
    if (video_plugin && !managed) {
        json_t *vid = config ? json_obj_get((json_t *)config, "video_gen") : NULL;
        if (!(vid && vid->type == JSON_OBJECT)) return false;
        const char *cur = tcp_str_or(vid, "provider", "");
        return cur && strcmp(cur, video_plugin) == 0;
    }

    if (managed) {
        json_t *feat = features
            ? json_copy((json_t *)features)
            : tools_config_nous_subscription_features(config, force_fresh);
        json_t *fmap = feat ? json_obj_get(feat, "features") : NULL;
        json_t *feature =
            fmap ? json_obj_get(fmap, managed) : NULL;
        bool managed_by_nous = feature ? tcp_bool_or(feature, "managed_by_nous", false)
                                       : false;

        bool result;
        if (strcmp(managed, "image_gen") == 0) {
            result = managed_by_nous;
            json_t *img = config ? json_obj_get((json_t *)config, "image_gen") : NULL;
            if (img && img->type == JSON_OBJECT) {
                const char *cp = tcp_str_or(img, "provider", "");
                if (tcp_has_key(img, "provider")) {
                    if (!(cp[0] == '\0' || strcmp(cp, "fal") == 0 ||
                          strcmp(cp, NOUS_MANAGED_PROVIDER) == 0))
                        result = false;
                }
                if (result &&
                    strcmp(cp, NOUS_MANAGED_PROVIDER) != 0 &&
                    json_obj_get(img, "use_gateway") != NULL &&
                    !tcp_truthy(json_obj_get(img, "use_gateway"), false))
                    result = false;
            }
        } else if (strcmp(managed, "video_gen") == 0) {
            result = managed_by_nous;
            json_t *vid = config ? json_obj_get((json_t *)config, "video_gen") : NULL;
            if (vid && vid->type == JSON_OBJECT) {
                const char *cp = tcp_str_or(vid, "provider", "");
                if (tcp_has_key(vid, "provider")) {
                    if (!(cp[0] == '\0' || strcmp(cp, "fal") == 0 ||
                          strcmp(cp, NOUS_MANAGED_PROVIDER) == 0))
                        result = false;
                }
                if (result &&
                    strcmp(cp, NOUS_MANAGED_PROVIDER) != 0 &&
                    json_obj_get(vid, "use_gateway") != NULL &&
                    !tcp_truthy(json_obj_get(vid, "use_gateway"), false))
                    result = false;
            }
        } else if (tcp_prov_str(provider, "tts_provider")) {
            const char *tts_provider = tcp_prov_str(provider, "tts_provider");
            const char *cur = tcp_cfg_get(config, "tts", "provider");
            result = managed_by_nous && cur &&
                     (strcmp(cur, tts_provider) == 0 ||
                      strcmp(cur, NOUS_MANAGED_PROVIDER) == 0);
        } else if (tcp_prov_str(provider, "stt_provider")) {
            const char *stt_provider = tcp_prov_str(provider, "stt_provider");
            const char *cur = tcp_cfg_get(config, "stt", "provider");
            result = managed_by_nous && cur &&
                     (strcmp(cur, stt_provider) == 0 ||
                      strcmp(cur, NOUS_MANAGED_PROVIDER) == 0);
        } else if (tcp_has_key(provider, "browser_provider")) {
            const char *bp = tcp_prov_str(provider, "browser_provider");
            const char *cur = tcp_cfg_get(config, "browser", "cloud_provider");
            result = managed_by_nous && cur &&
                     ((bp && strcmp(cur, bp) == 0) ||
                      strcmp(cur, NOUS_MANAGED_PROVIDER) == 0);
        } else if (tcp_prov_str(provider, "web_backend")) {
            const char *wb = tcp_prov_str(provider, "web_backend");
            const char *cur = tcp_cfg_get(config, "web", "backend");
            result = managed_by_nous && cur &&
                     ((wb && strcmp(cur, wb) == 0) ||
                      strcmp(cur, NOUS_MANAGED_PROVIDER) == 0) &&
                     tools_config_web_tier_matches(provider, config);
        } else {
            result = managed_by_nous;
        }
        json_free(feat);
        return result;
    }

    if (tcp_prov_str(provider, "tts_provider"))
        return strcmp(tcp_cfg_get(config, "tts", "provider"),
                      tcp_prov_str(provider, "tts_provider")) == 0;
    if (tcp_prov_str(provider, "stt_provider")) {
        const char *cur = tcp_cfg_get(config, "stt", "provider");
        const char *eff = (cur && cur[0]) ? cur : "local";
        return strcmp(eff, tcp_prov_str(provider, "stt_provider")) == 0;
    }
    if (tcp_has_key(provider, "browser_provider")) {
        const char *bp = tcp_prov_str(provider, "browser_provider");
        const char *cur = tcp_cfg_get(config, "browser", "cloud_provider");
        return bp && cur && strcmp(bp, cur) == 0;
    }
    if (tcp_prov_str(provider, "browser_backend")) {
        const char *bb = tcp_prov_str(provider, "browser_backend");
        const char *cur = tcp_cfg_get(config, "browser", "backend");
        if (cur && strcmp(cur, "False") == 0) cur = "off";   /* YAML 1.1 off */
        if (cur && strcmp(cur, bb) == 0) return true;
        if (cur && cur[0]) return false;   /* explicit other choice wins */
        if (strcmp(bb, "browser-use") != 0) return false;
        /* Backend unset: Browser Use mode is the default. Legacy cloud config
         * or a runnable CLI with no Camofox keeps the row active. */
        extern bool bu_is_legacy_browser_use_cloud_config(const char *cfg_json);
        json_t *bcfg = config ? json_obj_get((json_t *)config, "browser") : NULL;
        char *serialized = bcfg ? json_dumps(bcfg, 0) : NULL;
        bool legacy = bu_is_legacy_browser_use_cloud_config(serialized ? serialized : "{}");
        free(serialized);
        if (legacy) return true;
        char cli_path[1024];
        return bu_find_cli(cli_path, sizeof cli_path) == 1;
    }
    if (tcp_prov_str(provider, "web_backend")) {
        const char *wb = tcp_prov_str(provider, "web_backend");
        const char *cur = tcp_cfg_get(config, "web", "backend");
        if (!(cur && wb && strcmp(cur, wb) == 0)) return false;
        return tools_config_web_tier_matches(provider, config);
    }
    if (tcp_prov_str(provider, "computer_use_backend")) {
        const char *cb = tcp_prov_str(provider, "computer_use_backend");
        const char *cur = tcp_cfg_get(config, "computer_use", "backend");
        return cur && cb && strcmp(cur, cb) == 0;
    }
    if (tcp_prov_str(provider, "imagegen_backend")) {
        json_t *img = config ? json_obj_get((json_t *)config, "image_gen") : NULL;
        if (!(img && img->type == JSON_OBJECT)) return false;
        const char *cp = tcp_str_or(img, "provider", "");
        return strcmp(tcp_prov_str(provider, "imagegen_backend"), "fal") == 0 &&
               (cp[0] == '\0' || strcmp(cp, "fal") == 0) &&
               !tcp_truthy(json_obj_get(img, "use_gateway"), false);
    }
    return false;
}

/* _detect_active_provider_index analog (used by the configure flows). */
static int tcp_detect_active_provider_index(const json_t *providers,
                                            const json_t *config,
                                            bool force_fresh) {
    size_t n = json_len(providers);
    for (size_t i = 0; i < n; i++) {
        json_t *p = json_get(providers, (int)i);
        if (tools_config_is_provider_active(p, config, force_fresh, NULL))
            return (int)i;
        if (tcp_all_env_vars_set(p)) return (int)i;
    }
    return 0;
}

/* ── configuration gate ────────────────────────────────────────────────── */

/* PoP: tools_config_toolset_needs_configuration_prompt @ hermes_cli/tools_config.py:_toolset_needs_configuration_prompt */
bool tools_config_toolset_needs_configuration_prompt(const char *ts_key,
                                                     const json_t *config,
                                                     bool force_fresh) {
    json_t *cats = tools_config_tool_categories();
    json_t *cat = cats ? json_obj_get(cats, ts_key) : NULL;
    if (!cat)
        return !tools_config_toolset_has_keys(ts_key, config, force_fresh);

    json_t *visible = tools_config_visible_providers(cat, config, force_fresh, NULL);
    size_t n = json_len(visible);
    for (size_t i = 0; i < n; i++) {
        json_t *provider = json_get(visible, (int)i);
        const char *post_setup = tcp_prov_str(provider, "post_setup");
        if (post_setup && !tools_config_post_setup_already_installed(post_setup)) {
            json_free(visible);
            return true;
        }
    }
    json_free(visible);

    if (strcmp(ts_key, "tts") == 0) {
        json_t *c = config ? json_obj_get((json_t *)config, "tts") : NULL;
        return !(c && c->type == JSON_OBJECT) || !tcp_has_key(c, "provider");
    }
    if (strcmp(ts_key, "web") == 0) {
        json_t *c = config ? json_obj_get((json_t *)config, "web") : NULL;
        return !(c && c->type == JSON_OBJECT) || !tcp_has_key(c, "backend");
    }
    if (strcmp(ts_key, "browser") == 0) {
        json_t *c = config ? json_obj_get((json_t *)config, "browser") : NULL;
        return !(c && c->type == JSON_OBJECT) || !tcp_has_key(c, "cloud_provider");
    }
    if (strcmp(ts_key, "image_gen") == 0) {
        /* Satisfied when any registered image gen provider reports available.
         * (The in-tree FAL availability check runs through the same probe.) */
        size_t cnt = (size_t)image_gen_provider_count();
        for (size_t i = 0; i < cnt; i++) {
            const image_gen_provider_t *p =
                image_gen_get_provider_by_index((int)i);
            if (p && p->is_available && p->is_available()) return false;
        }
        return true;
    }
    if (strcmp(ts_key, "video_gen") == 0) {
        size_t cnt = (size_t)video_gen_provider_count();
        for (size_t i = 0; i < cnt; i++) {
            const video_gen_provider_t *p =
                video_gen_get_provider_by_index((int)i);
            if (p && p->is_available && p->is_available()) return false;
        }
        return true;
    }
    return !tools_config_toolset_has_keys(ts_key, config, force_fresh);
}

/* ── model pickers ─────────────────────────────────────────────────────── */

/* _format_imagegen_model_row analog (row without leading two spaces). */
static void tcp_format_model_row(char *out, size_t sz, const char *model_id,
                                 const json_t *meta,
                                 size_t w_model, size_t w_speed,
                                 size_t w_strengths) {
    snprintf(out, sz, "%-*s  %-*s  %-*s  %s",
             (int)w_model, model_id,
             (int)w_speed, tcp_str_or(meta, "speed", ""),
             (int)w_strengths, tcp_str_or(meta, "strengths", ""),
             tcp_str_or(meta, "price", ""));
}

static size_t tcp_max_len(json_t *catalog, const char *field, size_t def) {
    size_t best = def;
    size_t n = json_object_size(catalog);
    bool any = false;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_object_get_at(catalog, i);
        const char *s = v ? tcp_str_or(v, field, "") : "";
        if (strlen(s) > best) best = strlen(s);
        any = true;
    }
    (void)any;
    return best;
}

static size_t tcp_max_key_len(json_t *catalog) {
    size_t best = 0;
    size_t n = json_object_size(catalog);
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(catalog, i);
        if (k) {
            size_t l = strlen(k);
            if (l > best) best = l;
        }
    }
    return best;
}

/* Shared picker body over a catalog dict {model_id: meta} + default. */
static void tcp_pick_model_common(json_t *catalog, const char *default_model,
                                  json_t *section, const char *title) {
    if (!catalog || json_object_size(catalog) == 0) return;

    const char *cur = section ? tcp_str_or(section, "model", "") : "";
    char current[512];
    snprintf(current, sizeof current, "%s",
             (cur && cur[0]) ? cur : (default_model ? default_model : ""));
    if (!json_obj_get(catalog, current)) {
        /* Stale cross-provider model or drifted default → catalog default,
         * else first key. */
        const char *d = (default_model && json_obj_get(catalog, default_model))
                            ? default_model
                            : json_object_get_key_at(catalog, 0);
        snprintf(current, sizeof current, "%s", d ? d : "");
    }

    /* ordered = [current] + rest in catalog order */
    size_t n = json_object_size(catalog);
    const char **ordered = (const char **)calloc(n + 1, sizeof *ordered);
    size_t w_model = tcp_max_key_len(catalog);
    size_t w_speed = tcp_max_len(catalog, "speed", 6);
    size_t w_strengths = tcp_max_len(catalog, "strengths", 0);
    if (!ordered) return;

    size_t k = 0;
    ordered[k++] = current;
    for (size_t i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(catalog, i);
        if (key && strcmp(key, current) != 0) ordered[k++] = key;
    }

    printf("\n");
    char header[1024];
    snprintf(header, sizeof header, "  %-*s  %-*s  %-*s  Price",
             (int)w_model, "Model", (int)w_speed, "Speed",
             (int)w_strengths, "Strengths");
    tcp_print_color_line(header);

    char **rows = (char **)calloc(k + 1, sizeof *rows);
    if (!rows) { free(ordered); return; }
    for (size_t i = 0; i < k; i++) {
        json_t *meta = json_obj_get(catalog, ordered[i]);
        rows[i] = (char *)malloc(2048);
        if (!rows[i]) continue;
        tcp_format_model_row(rows[i], 2048, ordered[i], meta,
                             w_model, w_speed, w_strengths);
        if (strcmp(ordered[i], current) == 0)
            strncat(rows[i], "  ← currently in use", 2048 - strlen(rows[i]) - 1);
    }

    int idx = tcp_prompt_choice(title, rows, (int)k, 0);

    const char *chosen = ordered[idx >= 0 && idx < (int)k ? idx : 0];
    if (section) json_set(section, "model", json_string(chosen));
    char msg[640];
    snprintf(msg, sizeof msg, "  Model set to: %s", chosen);
    display_print_success(msg);

    for (size_t i = 0; i < k; i++) free(rows[i]);
    free(rows);
    free(ordered);
}

/* IMAGEGEN_BACKENDS = {"fal": {...}} — the only registered backend. */
/* PoP: tools_config_configure_imagegen_model @ hermes_cli/tools_config.py:_configure_imagegen_model */
void tools_config_configure_imagegen_model(const char *backend_name,
                                           json_t *config) {
    if (!backend_name || strcmp(backend_name, "fal") != 0)
        return;   /* backend not in IMAGEGEN_BACKENDS */
    json_t *catalog = NULL;
    char *default_model = NULL;
    if (!tools_config_fal_model_catalog(&catalog, &default_model)) return;

    json_t *cfg = config ? json_obj_get(config, "image_gen") : NULL;
    if (!(cfg && cfg->type == JSON_OBJECT)) {
        cfg = json_object();
        json_set(config, "image_gen", cfg);
    }
    tcp_pick_model_common(catalog, default_model, cfg,
                          "  Choose FAL.ai model:");
    json_free(catalog);
    free(default_model);
}

/* PoP: tools_config_plugin_image_gen_catalog @ hermes_cli/tools_config.py:_plugin_image_gen_catalog */
json_t *tools_config_plugin_image_gen_catalog(const char *plugin_name,
                                              char **default_model) {
    if (default_model) *default_model = NULL;
    json_t *empty = json_object();
    if (!plug_ensure_discovered() || !plugin_name) return empty;
    extern const char *cli_agent_image_gen_provider_default_model(const char *);
    extern char *cli_agent_image_gen_provider_list_models(const char *);
    char *models_json = cli_agent_image_gen_provider_list_models(plugin_name);
    if (!models_json) return empty;
    json_t *models = json_parse(models_json, NULL);
    free(models_json);
    if (!models || models->type != JSON_ARRAY) {
        if (models) json_free(models);
        return empty;
    }
    json_t *catalog = json_object();
    size_t n = json_len(models);
    for (size_t i = 0; i < n; i++) {
        json_t *m = json_get(models, (int)i);
        const char *id = m ? json_get_str(m, "id", NULL) : NULL;
        if (m && m->type == JSON_OBJECT && id) json_set(catalog, id, json_copy(m));
    }
    json_free(models);
    if (json_object_size(catalog) == 0) { json_free(catalog); return empty; }
    const char *def = cli_agent_image_gen_provider_default_model(plugin_name);
    if (default_model && def) *default_model = tcp_strdup(def);
    return catalog;
}
/* PoP: tools_config_configure_imagegen_model_for_plugin @ hermes_cli/tools_config.py:_configure_imagegen_model_for_plugin */
void tools_config_configure_imagegen_model_for_plugin(const char *plugin_name,
                                                      json_t *config) {
    char *default_model = NULL;
    json_t *catalog = tools_config_plugin_image_gen_catalog(plugin_name, &default_model);
    json_t *cfg = config ? json_obj_get(config, "image_gen") : NULL;
    if (!(cfg && cfg->type == JSON_OBJECT)) {
        cfg = json_object();
        json_set(config, "image_gen", cfg);
    }
    char title[256];
    snprintf(title, sizeof title, "  Choose %s model:", plugin_name);
    tcp_pick_model_common(catalog, default_model, cfg, title);
    json_free(catalog);
    free(default_model);
}

/* PoP: tools_config_configure_xai_imagine_storage @ hermes_cli/tools_config.py:_configure_xai_imagine_storage */
void tools_config_configure_xai_imagine_storage(const char *section_name,
                                                json_t *config) {
    json_t *section = config ? json_obj_get(config, section_name) : NULL;
    if (!(section && section->type == JSON_OBJECT)) {
        section = json_object();
        json_set(config, section_name, section);
    }
    json_t *xai = json_obj_get(section, "xai");
    if (!(xai && xai->type == JSON_OBJECT)) {
        xai = json_object();
        json_set(section, "xai", xai);
    }
    json_t *storage = json_obj_get(xai, "storage");
    if (!(storage && storage->type == JSON_OBJECT)) {
        storage = json_object();
        json_set(xai, "storage", storage);
    }

    display_print_warning(
        "  xAI Imagine can store generated media and create reusable public URLs. "
        "xAI may bill for stored files and public URL hosting.");
    char *choices[] = {
        (char *)"Enable public URLs without automatic expiry (recommended)",
        (char *)"Disable stored public URLs",
        (char *)"Enable public URLs for 2 days",
    };
    int idx = tcp_prompt_choice("  Stored public URLs:", choices, 3, 0);
    if (idx == 1) {
        json_set(storage, "enabled", json_bool(false));
        display_print_success("  xAI stored public URLs disabled");
    } else if (idx == 2) {
        json_set(storage, "enabled", json_bool(true));
        json_set(storage, "public_url", json_bool(true));
        json_set(storage, "expires_after", json_number((double)(2 * 24 * 60 * 60)));
        display_print_success("  xAI stored public URLs enabled for 2 days");
    } else {
        json_set(storage, "enabled", json_bool(true));
        json_set(storage, "public_url", json_bool(true));
        json_set(storage, "expires_after", json_null());
        display_print_success("  xAI stored public URLs enabled without automatic expiry");
    }
}

/* PoP: tools_config_select_plugin_image_gen_provider @ hermes_cli/tools_config.py:_select_plugin_image_gen_provider */
void tools_config_select_plugin_image_gen_provider(const char *plugin_name,
                                                   json_t *config,
                                                   bool use_gateway) {
    json_t *img = config ? json_obj_get(config, "image_gen") : NULL;
    if (!(img && img->type == JSON_OBJECT)) {
        img = json_object();
        json_set(config, "image_gen", img);
    }
    json_set(img, "provider",
             json_string(use_gateway ? NOUS_MANAGED_PROVIDER : plugin_name));
    json_obj_del(img, "use_gateway");
    char msg[256];
    snprintf(msg, sizeof msg, "  image_gen.provider set to: %s",
             use_gateway ? NOUS_MANAGED_PROVIDER : plugin_name);
    display_print_success(msg);
    tools_config_configure_imagegen_model_for_plugin(plugin_name, config);
    if (strcmp(plugin_name, "xai") == 0)
        tools_config_configure_xai_imagine_storage("image_gen", config);
}

/* PoP: tools_config_plugin_video_gen_catalog @ hermes_cli/tools_config.py:_plugin_video_gen_catalog */
json_t *tools_config_plugin_video_gen_catalog(const char *plugin_name,
                                              char **default_model) {
    if (default_model) *default_model = NULL;
    json_t *empty = json_object();
    if (!plug_ensure_discovered() || !plugin_name) return empty;
    extern char *vgp_list_models(void);
    extern char *vgp_default_model(void);
    char *models_json = vgp_list_models();
    if (!models_json) return empty;
    json_t *models = json_parse(models_json, NULL);
    free(models_json);
    if (!models || models->type != JSON_ARRAY) {
        if (models) json_free(models);
        return empty;
    }
    json_t *catalog = json_object();
    size_t n = json_len(models);
    for (size_t i = 0; i < n; i++) {
        json_t *m = json_get(models, (int)i);
        const char *id = m ? json_get_str(m, "id", NULL) : NULL;
        if (m && m->type == JSON_OBJECT && id) json_set(catalog, id, json_copy(m));
    }
    json_free(models);
    if (json_object_size(catalog) == 0) { json_free(catalog); return empty; }
    char *def = vgp_default_model();
    if (default_model && def) *default_model = tcp_strdup(def);
    free(def);
    return catalog;
}
/* PoP: tools_config_configure_videogen_model_for_plugin @ hermes_cli/tools_config.py:_configure_videogen_model_for_plugin */
void tools_config_configure_videogen_model_for_plugin(const char *plugin_name,
                                                      json_t *config) {
    char *default_model = NULL;
    json_t *catalog = tools_config_plugin_video_gen_catalog(plugin_name, &default_model);
    json_t *cfg = config ? json_obj_get(config, "video_gen") : NULL;
    if (!(cfg && cfg->type == JSON_OBJECT)) {
        cfg = json_object();
        json_set(config, "video_gen", cfg);
    }
    char title[256];
    snprintf(title, sizeof title, "  Choose %s model:", plugin_name);
    tcp_pick_model_common(catalog, default_model, cfg, title);
    json_free(catalog);
    free(default_model);
}

/* STT_MODEL_CATALOG + _STT_MODEL_CONFIG_KEY */
static const struct {
    const char *provider;
    const char *const *models;
} STT_MODEL_CATALOG[] = {
    { "local",      (const char *[]){ "base", "tiny", "small", "medium", "large-v3", NULL } },
    { "groq",       (const char *[]){ "whisper-large-v3-turbo", "whisper-large-v3", "distil-whisper-large-v3-en", NULL } },
    { "openai",     (const char *[]){ "whisper-1", "gpt-4o-mini-transcribe", "gpt-4o-transcribe", "gpt-transcribe", NULL } },
    { "elevenlabs", (const char *[]){ "scribe_v2", "scribe_v1", NULL } },
    { NULL, NULL },
};

/* PoP: tools_config_configure_stt_model @ hermes_cli/tools_config.py:_configure_stt_model */
void tools_config_configure_stt_model(const char *stt_provider, json_t *config) {
    const char *const *catalog = NULL;
    for (size_t i = 0; STT_MODEL_CATALOG[i].provider; i++) {
        if (strcmp(STT_MODEL_CATALOG[i].provider, stt_provider) == 0) {
            catalog = STT_MODEL_CATALOG[i].models;
            break;
        }
    }
    if (!catalog) return;   /* no static catalog → skip the prompt */

    json_t *stt_cfg = config ? json_obj_get(config, "stt") : NULL;
    if (!(stt_cfg && stt_cfg->type == JSON_OBJECT)) {
        stt_cfg = json_object();
        json_set(config, "stt", stt_cfg);
    }
    json_t *prov_cfg = json_obj_get(stt_cfg, stt_provider);
    if (!(prov_cfg && prov_cfg->type == JSON_OBJECT)) {
        prov_cfg = json_object();
        json_set(stt_cfg, stt_provider, prov_cfg);
    }
    /* ElevenLabs historically uses model_id. */
    const char *model_key =
        strcmp(stt_provider, "elevenlabs") == 0 ? "model_id" : "model";
    const char *current = tcp_str_or(prov_cfg, model_key, "");

    size_t n = 0;
    while (catalog[n]) n++;
    int default_idx = 0;
    for (size_t i = 0; i < n; i++)
        if (strcmp(catalog[i], current) == 0) { default_idx = (int)i; break; }

    char **choices = (char **)calloc(n + 1, sizeof *choices);
    if (!choices) return;
    for (size_t i = 0; i < n; i++) choices[i] = (char *)catalog[i];
    int idx = tcp_prompt_choice("  Select STT model:", choices, (int)n,
                                default_idx);
    free(choices);
    if (idx < 0 || idx >= (int)n) idx = 0;
    json_set(prov_cfg, model_key, json_string(catalog[idx]));
    char msg[192];
    snprintf(msg, sizeof msg, "  STT model set to: %s", catalog[idx]);
    display_print_success(msg);
}

/* PoP: tools_config_select_plugin_video_gen_provider @ hermes_cli/tools_config.py:_select_plugin_video_gen_provider */
void tools_config_select_plugin_video_gen_provider(const char *plugin_name,
                                                   json_t *config,
                                                   bool use_gateway) {
    json_t *vid = config ? json_obj_get(config, "video_gen") : NULL;
    if (!(vid && vid->type == JSON_OBJECT)) {
        vid = json_object();
        json_set(config, "video_gen", vid);
    }
    json_set(vid, "provider",
             json_string(use_gateway ? NOUS_MANAGED_PROVIDER : plugin_name));
    json_obj_del(vid, "use_gateway");
    char msg[256];
    snprintf(msg, sizeof msg, "  video_gen.provider set to: %s",
             use_gateway ? NOUS_MANAGED_PROVIDER : plugin_name);
    display_print_success(msg);
    tools_config_configure_videogen_model_for_plugin(plugin_name, config);
    if (strcmp(plugin_name, "xai") == 0)
        tools_config_configure_xai_imagine_storage("video_gen", config);
}

/* ── config writing + configure flows ──────────────────────────────────── */

static json_t *tcp_setdefault_section(json_t *config, const char *key) {
    json_t *sec = config ? json_obj_get(config, key) : NULL;
    if (!(sec && sec->type == JSON_OBJECT)) {
        sec = json_object();
        json_set(config, key, sec);
    }
    return sec;
}

/* _set_selection closure analog. */
static void tcp_set_selection(json_t *config, const char *section_key,
                              const char *name_key, const char *vendor_value,
                              const char *managed_feature) {
    json_t *section = tcp_setdefault_section(config, section_key);
    json_set(section, name_key,
             json_string(managed_feature ? NOUS_MANAGED_PROVIDER : vendor_value));
    json_obj_del(section, "use_gateway");
}

/* PoP: tools_config_write_provider_config @ hermes_cli/tools_config.py:_write_provider_config */
void tools_config_write_provider_config(const json_t *provider, json_t *config,
                                        const char *managed_feature) {
    const char *tts = tcp_prov_str(provider, "tts_provider");
    if (tts) tcp_set_selection(config, "tts", "provider", tts, managed_feature);

    const char *stt = tcp_prov_str(provider, "stt_provider");
    if (stt) tcp_set_selection(config, "stt", "provider", stt, managed_feature);

    if (tcp_has_key(provider, "browser_provider")) {
        const char *bp = tcp_prov_str(provider, "browser_provider");
        json_t *browser_cfg = tcp_setdefault_section(config, "browser");
        if ((bp && bp[0]) || managed_feature)
            tcp_set_selection(config, "browser", "cloud_provider", bp, managed_feature);
        else
            json_obj_del(browser_cfg, "use_gateway");
    }

    const char *bb = tcp_prov_str(provider, "browser_backend");
    if (bb) {
        json_t *browser_cfg = tcp_setdefault_section(config, "browser");
        json_set(browser_cfg, "backend", json_string(bb));
    }

    const char *wb = tcp_prov_str(provider, "web_backend");
    if (wb) {
        tcp_set_selection(config, "web", "backend", wb, managed_feature);
        json_t *web_cfg = json_obj_get(config, "web");
        if (web_cfg && web_cfg->type == JSON_OBJECT) {
            const char *tier = tcp_prov_str(provider, "web_tier");
            if (tier && tier[0]) {
                json_t *tiers = json_obj_get(web_cfg, "provider_tier");
                if (!(tiers && tiers->type == JSON_OBJECT)) {
                    tiers = json_object();
                    json_set(web_cfg, "provider_tier", tiers);
                }
                if (tiers && tiers->type == JSON_OBJECT)
                    json_set(tiers, wb, json_string(tier));
            } else {
                json_t *stale = json_obj_get(web_cfg, "provider_tier");
                if (stale && stale->type == JSON_OBJECT)
                    json_obj_del(stale, wb);
            }
        }
    }

    const char *cu = tcp_prov_str(provider, "computer_use_backend");
    if (cu) {
        json_t *cu_cfg = tcp_setdefault_section(config, "computer_use");
        json_set(cu_cfg, "backend", json_string(cu));
    }

    /* Managed rows for categories without a marker still persist "nous". */
    if (managed_feature && strcmp(managed_feature, "web") != 0 &&
        strcmp(managed_feature, "tts") != 0 &&
        strcmp(managed_feature, "stt") != 0 &&
        strcmp(managed_feature, "browser") != 0) {
        json_t *section = tcp_setdefault_section(config, managed_feature);
        json_set(section, "provider", json_string(NOUS_MANAGED_PROVIDER));
        json_obj_del(section, "use_gateway");
    } else if (!managed_feature) {
        /* Clear any stale legacy use_gateway on the category resolved from
         * the provider's own markers first, then the category walk. */
        static const struct { const char *marker; const char *section; }
            MARKERS[] = {
                { "tts_provider", "tts" }, { "stt_provider", "stt" },
                { "browser_provider", "browser" }, { "web_backend", "web" },
                { "image_gen_plugin_name", "image_gen" },
                { "imagegen_backend", "image_gen" },
                { "video_gen_plugin_name", "video_gen" },
                { NULL, NULL },
            };
        bool cleared = false;
        for (size_t i = 0; MARKERS[i].marker; i++) {
            if (tcp_prov_str(provider, MARKERS[i].marker) ||
                tcp_has_key(provider, MARKERS[i].marker)) {
                json_t *section = json_obj_get(config, MARKERS[i].section);
                if (section && section->type == JSON_OBJECT)
                    json_obj_del(section, "use_gateway");
                cleared = true;
            }
        }
        if (!cleared) {
            json_t *cats = tools_config_tool_categories();
            if (cats) {
                size_t cn = json_object_size(cats);
                for (size_t i = 0; i < cn; i++) {
                    json_t *cat = json_object_get_at(cats, i);
                    json_t *provs = cat ? json_obj_get(cat, "providers") : NULL;
                    bool found = false;
                    if (provs && provs->type == JSON_ARRAY) {
                        size_t pn = json_len(provs);
                        for (size_t j = 0; j < pn; j++) {
                            json_t *p = json_get(provs, (int)j);
                            if (json_serialize(p) && json_serialize((json_t *)provider) &&
                                strcmp(json_serialize(p), json_serialize((json_t *)provider)) == 0) {
                                found = true;
                                break;
                            }
                        }
                    }
                    if (found) {
                        const char *cat_key =
                            json_object_get_key_at(cats, i);
                        json_t *section =
                            cat_key ? json_obj_get(config, cat_key) : NULL;
                        if (section && section->type == JSON_OBJECT)
                            json_obj_del(section, "use_gateway");
                        break;
                    }
                }
            }
        }
    }
}

/* PoP: tools_config_apply_provider_selection @ hermes_cli/tools_config.py:apply_provider_selection */
void tools_config_apply_provider_selection(const char *ts_key,
                                           const char *provider_name,
                                           json_t *config) {
    json_t *cats = tools_config_tool_categories();
    json_t *cat = (cats && ts_key) ? json_obj_get(cats, ts_key) : NULL;
    if (!cat) {
        char msg[320];
        snprintf(msg, sizeof msg, "KeyError: Toolset has no configurable category: %s",
                 ts_key ? ts_key : "");
        display_print_error(msg);
        return;
    }
    json_t *providers = tools_config_visible_providers(cat, config, true, NULL);
    size_t n = json_len(providers);
    json_t *match = NULL;
    for (size_t i = 0; i < n; i++) {
        json_t *p = json_get(providers, (int)i);
        const char *nm = p ? json_get_str(p, "name", NULL) : NULL;
        if (nm && provider_name && strcmp(nm, provider_name) == 0) { match = p; break; }
    }
    if (!match) {
        char msg[512];
        snprintf(msg, sizeof msg,
                 "KeyError: Unknown provider %s for toolset %s",
                 provider_name ? provider_name : "", ts_key ? ts_key : "");
        display_print_error(msg);
        json_free(providers);
        return;
    }
    const char *managed = tcp_prov_str(match, "managed_nous_feature");
    tools_config_write_provider_config(match, config, managed);

    const char *plugin_name = tcp_prov_str(match, "image_gen_plugin_name");
    if (plugin_name) {
        json_t *img = tcp_setdefault_section(config, "image_gen");
        json_set(img, "provider",
                 json_string(managed ? NOUS_MANAGED_PROVIDER : plugin_name));
        json_obj_del(img, "use_gateway");
    }
    const char *video_plugin = tcp_prov_str(match, "video_gen_plugin_name");
    if (video_plugin) {
        json_t *vid = tcp_setdefault_section(config, "video_gen");
        json_set(vid, "provider",
                 json_string(managed ? NOUS_MANAGED_PROVIDER : video_plugin));
        json_obj_del(vid, "use_gateway");
    }
    const char *ib = tcp_prov_str(match, "imagegen_backend");
    if (ib && !managed) {
        json_t *img = tcp_setdefault_section(config, "image_gen");
        json_set(img, "provider", json_string("fal"));
        json_obj_del(img, "use_gateway");
    }
    json_free(providers);
}

/* Nous Portal gate shared by configure/reconfigure: managed rows drive an
 * inline login; pure pre-auth UX rows keep the entitlement check. Returns
 * true when the caller may proceed. */
static bool tcp_nous_gate(const json_t *provider, json_t *config,
                          bool force_fresh) {
    const char *managed = tcp_prov_str(provider, "managed_nous_feature");
    if (managed) {
        /* ensure_nous_portal_access(): the inline Portal login runs through
         * the auth surface; entitlement is mirrored on the config document
         * (nous_account_info.tool_gateway_entitled). Deny without it. */
        json_t *features =
            tools_config_nous_subscription_features(config, force_fresh);
        json_t *acct = features ? json_obj_get(features, "account_info") : NULL;
        bool ok = acct && tcp_bool_or(acct, "logged_in", false) &&
                  tcp_bool_or(acct, "tool_gateway_entitled", false);
        json_free(features);
        if (!ok) {
            display_print_warning(
                "  Not enabled — Nous Portal access is required for this backend.");
            return false;
        }
        return true;
    }
    if (tcp_bool_or(provider, "requires_nous_auth", false)) {
        json_t *features =
            tools_config_nous_subscription_features(config, force_fresh);
        bool auth_present =
            features ? tcp_bool_or(features, "nous_auth_present", false) : false;
        json_t *acct = features ? json_obj_get(features, "account_info") : NULL;
        bool entitled = acct ? tcp_bool_or(acct, "paid_service_access", false)
                             : false;
        json_free(features);
        if (!auth_present || !entitled) {
            char msg[640];
            snprintf(msg, sizeof msg,
                     "  %s",
                     tcp_str_or(provider, "name",
                                "Nous Subscription is only available after logging into Nous Portal."));
            display_print_warning(
                "  Nous Subscription is only available after logging into Nous Portal.");
            (void)msg;
            return false;
        }
    }
    return true;
}

/* PoP: tools_config_configure_provider @ hermes_cli/tools_config.py:_configure_provider */
void tools_config_configure_provider(const json_t *provider, json_t *config,
                                     bool force_fresh) {
    if (!provider || !config) return;

    json_t *env_vars = json_obj_get((json_t *)provider, "env_vars");
    size_t n_env = (env_vars && env_vars->type == JSON_ARRAY)
                       ? json_len(env_vars) : 0;
    const char *managed = tcp_prov_str(provider, "managed_nous_feature");

    if (!tcp_nous_gate(provider, config, force_fresh)) return;

    /* Selection messages + persistence via the shared write path. */
    const char *stt = tcp_prov_str(provider, "stt_provider");
    if (stt) {
        char msg[256];
        snprintf(msg, sizeof msg, "  STT provider set to: %s", stt);
        display_print_success(msg);
    }
    if (tcp_has_key(provider, "browser_provider")) {
        const char *bp = tcp_prov_str(provider, "browser_provider");
        if (bp && strcmp(bp, "local") == 0)
            display_print_success("  Browser set to local mode");
        else if (bp && bp[0]) {
            char msg[256];
            snprintf(msg, sizeof msg, "  Browser cloud provider set to: %s", bp);
            display_print_success(msg);
        }
    }
    if (tcp_prov_str(provider, "browser_backend"))
        display_print_success("  Browser set to Browser Use (browser_exec via CLI 3.0)");
    if (tcp_prov_str(provider, "web_backend")) {
        char msg[256];
        snprintf(msg, sizeof msg, "  Web backend set to: %s",
                 tcp_prov_str(provider, "web_backend"));
        display_print_success(msg);
    }

    tools_config_write_provider_config(provider, config, managed);

    if (n_env == 0) {
        const char *post_setup = tcp_prov_str(provider, "post_setup");
        if (post_setup) tools_config_run_post_setup(post_setup);
        char msg[320];
        snprintf(msg, sizeof msg, "  %s - no configuration needed!",
                 tcp_str_or(provider, "name", ""));
        display_print_success(msg);
        if (managed)
            display_print_info(
                "  Requests for this tool will be billed to your Nous subscription.");
        const char *plugin_name = tcp_prov_str(provider, "image_gen_plugin_name");
        if (plugin_name) {
            tools_config_select_plugin_image_gen_provider(plugin_name, config,
                                                          managed != NULL);
            return;
        }
        const char *video_plugin = tcp_prov_str(provider, "video_gen_plugin_name");
        if (video_plugin) {
            tools_config_select_plugin_video_gen_provider(video_plugin, config,
                                                          managed != NULL);
            return;
        }
        const char *backend = tcp_prov_str(provider, "imagegen_backend");
        if (backend) {
            tools_config_configure_imagegen_model(backend, config);
            json_t *img = json_obj_get(config, "image_gen");
            if (img && img->type == JSON_OBJECT) {
                json_set(img, "provider",
                         json_string(managed ? NOUS_MANAGED_PROVIDER : "fal"));
                json_obj_del(img, "use_gateway");
            }
        }
        if (stt && !managed)
            tools_config_configure_stt_model(stt, config);
        return;
    }

    /* Prompt for each required env var. */
    bool all_configured = true;
    for (size_t i = 0; i < n_env; i++) {
        json_t *var = json_get(env_vars, (int)i);
        const char *key = var ? json_get_str(var, "key", NULL) : NULL;
        if (!key) continue;
        char *existing = tcp_get_env(key);
        if (existing && existing[0]) {
            char msg[320];
            snprintf(msg, sizeof msg, "  %s: already configured", key);
            display_print_success(msg);
            free(existing);
            continue;
        }
        free(existing);
        const char *url = var ? json_get_str(var, "url", "") : "";
        if (url && url[0]) {
            char msg[384];
            snprintf(msg, sizeof msg, "  Get yours at: %s", url);
            display_print_info(msg);
        }
        const char *prompt_text =
            var ? json_get_str(var, "prompt", key) : key;
        char q[512];
        snprintf(q, sizeof q, "    %s", prompt_text ? prompt_text : key);
        char *value = tcp_prompt_hidden(q);
        if (value && value[0] && value[strspn(value, " \t")]) {
            config_save_env_value(key, value);
            display_print_success("    Saved");
        } else {
            display_print_warning("    Skipped");
            all_configured = false;
        }
        free(value);
    }

    const char *post_setup = tcp_prov_str(provider, "post_setup");
    if (post_setup && all_configured)
        tools_config_run_post_setup(post_setup);

    if (all_configured) {
        char msg[320];
        snprintf(msg, sizeof msg, "  %s configured!", tcp_str_or(provider, "name", ""));
        display_print_success(msg);
        const char *plugin_name = tcp_prov_str(provider, "image_gen_plugin_name");
        if (plugin_name) {
            tools_config_select_plugin_image_gen_provider(plugin_name, config,
                                                          managed != NULL);
            return;
        }
        const char *video_plugin = tcp_prov_str(provider, "video_gen_plugin_name");
        if (video_plugin) {
            tools_config_select_plugin_video_gen_provider(video_plugin, config,
                                                          managed != NULL);
            return;
        }
        const char *backend = tcp_prov_str(provider, "imagegen_backend");
        if (backend) {
            tools_config_configure_imagegen_model(backend, config);
            json_t *img = json_obj_get(config, "image_gen");
            if (img && img->type == JSON_OBJECT) {
                json_set(img, "provider",
                         json_string(managed ? NOUS_MANAGED_PROVIDER : "fal"));
                json_obj_del(img, "use_gateway");
            }
        }
        if (stt && !managed)
            tools_config_configure_stt_model(stt, config);
    }
}

/* ── vision backend ────────────────────────────────────────────────────── */

/* PoP: tools_config_configure_vision_provider_model @ hermes_cli/tools_config.py:_configure_vision_provider_model */
void tools_config_configure_vision_provider_model(json_t *config,
                                                  json_t *vision_cfg) {
    /* Provider rows come from the shared aux-picker substrate
     * (hermes_cli/inventory.py:build_aux_picker_rows). The C runtime does not
     * yet expose that substrate; mirror its ImportError guard faithfully. */
    (void)config; (void)vision_cfg;
    display_print_warning("  Could not load provider list.");
}

/* PoP: tools_config_configure_vision_backend @ hermes_cli/tools_config.py:_configure_vision_backend */
void tools_config_configure_vision_backend(void) {
    printf("\n");
    display_print_warning("  Vision / Image Analysis needs a multimodal model.");
    display_print_info("  Pick any provider + model (like /model), or let it auto-detect.");

    char *choices[] = {
        (char *)"Auto — use your main model / aggregator fallback (recommended)",
        (char *)"Pick a provider and model",
        (char *)"Custom OpenAI-compatible endpoint — base URL, API key, model",
        (char *)"Skip",
    };
    int idx = tcp_prompt_choice("  Configure vision backend", choices, 4, 0);

    extern json_t *config_py_load_config_impl(int want_deepcopy);
    extern int config_save_config(const char *config_json);
    json_t *config = config_py_load_config_impl(1);
    if (!config) return;
    json_t *aux = json_obj_get(config, "auxiliary");
    if (!(aux && aux->type == JSON_OBJECT)) {
        aux = json_object();
        json_set(config, "auxiliary", aux);
    }
    json_t *vision_cfg = json_obj_get(aux, "vision");
    if (!(vision_cfg && vision_cfg->type == JSON_OBJECT)) {
        vision_cfg = json_object();
        json_set(aux, "vision", vision_cfg);
    }

    if (idx == 0) {
        static const char *const KEYS[] = { "provider", "model", "base_url",
                                            "api_key", "api_mode", NULL };
        for (size_t i = 0; KEYS[i]; i++) json_obj_del(vision_cfg, KEYS[i]);
        {
            char *ser = json_dumps(config, 0);
            if (ser) { config_save_config(ser); free(ser); }
        }
        display_print_success("  Vision set to auto (main model / aggregator fallback)");
        return;
    }
    if (idx == 1) {
        tools_config_configure_vision_provider_model(config, vision_cfg);
        return;
    }
    if (idx == 2) {
        char *base_in = tcp_prompt_hidden("    Base URL (blank for OpenAI)");
        char base_url[512];
        snprintf(base_url, sizeof base_url, "%s", base_in ? base_in : "");
        free(base_in);
        for (char *p = base_url; *p; p++)
            if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
                memmove(p, p + 1, strlen(p));
                if (p[0] == '\0') break;
                p--;
            }
        bool is_native_openai = strstr(base_url, "//api.openai.com") != NULL &&
                                strstr(base_url, "api.openai.com") ==
                                    strchr(strstr(base_url, "//api.openai.com") + 2, '/');
        is_native_openai = base_url[0] && strstr(base_url, "api.openai.com") != NULL;
        if (!base_url[0]) strcpy(base_url, "https://api.openai.com/v1");
        (void)is_native_openai;
        bool native = strstr(base_url, "api.openai.com") != NULL;
        char *api_key = tcp_prompt_hidden(native ? "    OPENAI_API_KEY" : "    API key");
        if (!api_key || !api_key[strspn(api_key, " \t")]) {
            display_print_warning("    Skipped");
            free(api_key);
            return;
        }
        const char *default_model = native ? "gpt-4o-mini" : "";
        char prompt_q[512];
        snprintf(prompt_q, sizeof prompt_q, "    Vision model%s%s%s",
                 default_model[0] ? " (blank for " : "",
                 default_model[0] ? default_model : "",
                 default_model[0] ? ")" : "");
        char *model_in = tcp_prompt_hidden(prompt_q);
        char model[512];
        snprintf(model, sizeof model, "%s", model_in ? model_in : "");
        free(model_in);

        config_save_env_value("OPENAI_API_KEY", api_key);
        free(api_key);
        /* Only base_url + model go to config.yaml; the key is the secret. */
        json_set(vision_cfg, "provider", json_string("custom"));
        json_set(vision_cfg, "base_url", json_string(base_url));
        if (model[strspn(model, " \t")])
            json_set(vision_cfg, "model", json_string(model));
        else
            json_obj_del(vision_cfg, "model");
        {
            char *ser = json_dumps(config, 0);
            if (ser) { config_save_config(ser); free(ser); }
        }
        char msg[768];
        if (model[strspn(model, " \t")])
            snprintf(msg, sizeof msg, "  Vision set to custom endpoint (%s)", model);
        else
            snprintf(msg, sizeof msg, "  Vision set to custom endpoint");
        display_print_success(msg);
        return;
    }
    display_print_info("  Skipped vision configuration");
}

/* PoP: tools_config_configure_simple_requirements @ hermes_cli/tools_config.py:_configure_simple_requirements */
void tools_config_configure_simple_requirements(const char *ts_key) {
    if (!ts_key) return;
    if (strcmp(ts_key, "vision") == 0) {
        if (tools_config_toolset_has_keys("vision", NULL, false)) return;
        tools_config_configure_vision_backend();
        return;
    }
    json_t *reqs_doc = tools_config_toolset_env_requirements();
    json_t *requirements = reqs_doc ? json_obj_get(reqs_doc, ts_key) : NULL;
    if (!requirements || requirements->type != JSON_ARRAY ||
        json_len(requirements) == 0)
        return;

    /* missing = pairs whose env var is unset */
    size_t n = json_len(requirements);
    for (size_t i = 0; i < n; i++) {
        json_t *pair = json_get(requirements, (int)i);
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 1) continue;
        const char *var = json_string_value(json_get(pair, 0));
        const char *url = json_len(pair) > 1
                              ? json_string_value(json_get(pair, 1)) : NULL;
        if (!var || tcp_env_present(var)) continue;
        if (url && url[0]) {
            char msg[384];
            snprintf(msg, sizeof msg, "  Get key at: %s", url);
            display_print_info(msg);
        }
        char q[256];
        snprintf(q, sizeof q, "    %s", var);
        char *value = tcp_prompt_hidden(q);
        if (value && value[strspn(value, " \t")]) {
            config_save_env_value(var, value);
            display_print_success("    Saved");
        } else {
            display_print_warning("    Skipped");
        }
        free(value);
    }
}

/* PoP: tools_config_configure_tool_category @ hermes_cli/tools_config.py:_configure_tool_category */
void tools_config_configure_tool_category(const char *ts_key,
                                          const json_t *cat,
                                          json_t *config, bool force_fresh) {
    if (!cat || !config) return;
    const char *icon = tcp_str_or(cat, "icon", "");
    const char *name = tcp_str_or(cat, "name", "");
    json_t *providers = tools_config_visible_providers(cat, config, force_fresh, NULL);
    size_t n = json_len(providers);
    /* _hidden_nous_gateway_message is a deprecated no-op → "" always. */

    /* Python version requirement: the C runtime has no sys.version_info gate;
     * requires_python rows are gated by their check_fn at tool registration. */

    if (n == 1) {
        json_t *provider = json_get(providers, 0);
        printf("\n");
        char header[640];
        snprintf(header, sizeof header, "  --- %s %s (%s) ---", icon, name,
                 json_get_str(provider, "name", ""));
        tcp_print_color_line(header);
        const char *tag = json_get_str(provider, "tag", "");
        if (tag && tag[0]) {
            char msg[512];
            snprintf(msg, sizeof msg, "  %s", tag);
            display_print_info(msg);
        }
        const char *note = tcp_str_or(cat, "setup_note", "");
        if (note && note[0]) {
            char msg[768];
            snprintf(msg, sizeof msg, "  %s", note);
            display_print_info(msg);
        }
        tools_config_configure_provider(provider, config, force_fresh);
        json_free(providers);
        return;
    }

    printf("\n");
    const char *title = tcp_str_or(cat, "setup_title", "Choose a provider");
    {
        char header[768];
        snprintf(header, sizeof header, "  --- %s %s - %s ---", icon, name,
                 title);
        tcp_print_color_line(header);
    }
    {
        const char *note = tcp_str_or(cat, "setup_note", "");
        if (note && note[0]) {
            char msg[768];
            snprintf(msg, sizeof msg, "  %s", note);
            display_print_info(msg);
        }
    }
    printf("\n");

    bool nous_logged_in = false;
    {
        json_t *features =
            tools_config_nous_subscription_features(config, force_fresh);
        nous_logged_in =
            features ? tcp_bool_or(features, "nous_auth_present", false) : false;
        json_free(features);
    }

    size_t total_choices = n + 1;   /* + Skip */
    char **choices = (char **)calloc(total_choices + 1, sizeof *choices);
    if (!choices) { json_free(providers); return; }
    for (size_t i = 0; i < n; i++) {
        json_t *p = json_get(providers, (int)i);
        const char *badge = json_get_str(p, "badge", "");
        const char *ptag = json_get_str(p, "tag", "");
        const char *configured = "";
        if (!tcp_all_env_vars_set(p)) {
            configured = "";
        } else if (tools_config_is_provider_active(p, config, force_fresh, NULL)) {
            configured = " [active]";
        } else if (!tcp_has_key(p, "env_vars") ||
                   (json_obj_get((json_t *)p, "env_vars") &&
                    json_len(json_obj_get((json_t *)p, "env_vars")) == 0)) {
            configured = "";
        } else {
            configured = " [configured]";
        }
        const char *sub_marker = "";
        if (tcp_prov_str(p, "managed_nous_feature"))
            sub_marker = nous_logged_in
                ? "  ★ Included with your Nous subscription"
                : "  ★ via Nous Portal (login on select)";
        choices[i] = (char *)malloc(1024);
        snprintf(choices[i], 1024, "%s%s%s%s%s",
                 json_get_str(p, "name", ""),
                 (badge && badge[0]) ? " [" : "",
                 (badge && badge[0]) ? badge : "",
                 (badge && badge[0]) ? "]" : "",
                 "");
        if (ptag && ptag[0]) {
            char tmp[512];
            snprintf(tmp, sizeof tmp, " — %s", ptag);
            strncat(choices[i], tmp, 1024 - strlen(choices[i]) - 1);
        }
        strncat(choices[i], configured, 1024 - strlen(choices[i]) - 1);
        strncat(choices[i], sub_marker, 1024 - strlen(choices[i]) - 1);
    }
    choices[n] = tcp_strdup("Skip — keep defaults / configure later");

    int default_idx = tcp_detect_active_provider_index(providers, config,
                                                       force_fresh);
    int provider_idx = tcp_prompt_choice(title, choices, (int)total_choices,
                                         default_idx);

    for (size_t i = 0; i < total_choices; i++) free(choices[i]);
    free(choices);

    if (provider_idx < 0 || provider_idx >= (int)n) {
        char msg[256];
        snprintf(msg, sizeof msg, "  Skipped %s", name);
        display_print_info(msg);
        json_free(providers);
        return;
    }
    tools_config_configure_provider(json_get(providers, provider_idx), config,
                                    force_fresh);
    json_free(providers);
}
