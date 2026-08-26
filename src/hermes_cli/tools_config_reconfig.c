/* tools_config_reconfig.c — faithful C11 port of the reconfigure / MCP /
 * listing surface of hermes_cli/tools_config.py.
 *
 * Reuses: platform_tools (_get_platform_tools/_save_platform_tools),
 * tools_config_toolsets (effective toolsets, has_keys, enabled_for_reconfigure),
 * tools_config_providers (visible providers / activity / configure flows),
 * config ports (load/save), curses_ui checklist, registry of platforms.
 */
#define _GNU_SOURCE
#include "tools_config_reconfig.h"
#include "tools_config_providers.h"
#include "tools_config_toolsets.h"
#include "platform_tools.h"
#include "port_config_py_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "tools_config_install.h"
#include "tools_config_helpers.h"
#include "hermes_display.h"

/* ── externs from sibling ports ────────────────────────────────────────── */
extern int curses_radiolist(const char *title, char **items, int item_count,
                            int selected, int cancel_returns,
                            const char *description, int searchable);
extern char *stp_prompt(const char *question, const char *default_value);
extern int config_save_env_value(const char *key, const char *value);
extern json_t *config_py_load_config_impl(int want_deepcopy);
extern int config_save_config(const char *config_json);
extern bool plug_ensure_discovered(void);
extern char *plug_get_portable_mcp_server_names_nowait_json(void);

/* _toolset_enabled_for_reconfigure is defined in tools_config_toolsets.c. */
bool tools_config_toolset_enabled_for_reconfigure(const char *ts_key,
                                                  const json_t *config);

/* local helpers shared with providers.c are re-declared here minimally. */
/* cat.get(key) string with fallback */
static const char *tcp_helper_cat_str(const json_t *obj, const char *key,
                                      const char *def) {
    json_t *v = json_obj_get((json_t *)obj, key);
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0])
        return v->str_val;
    return def;
}

static bool tcp_helper_has_env_vars(const json_t *provider) {
    json_t *ev = json_obj_get((json_t *)provider, "env_vars");
    return ev && ev->type == JSON_ARRAY && json_len(ev) > 0;
}

static char *tcr_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static const char *tcr_prov_str(const json_t *provider, const char *key) {
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

static bool tcr_has_key(const json_t *obj, const char *key) {
    return obj && json_obj_get((json_t *)obj, key) != NULL;
}

static bool tcr_bool_or(const json_t *obj, const char *key, bool def) {
    json_t *v = json_obj_get((json_t *)obj, key);
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        if (!s[0]) return def;
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

static char *tcr_get_env(const char *key) {
    return config_py_get_env_value_prefer_dotenv(key);
}

static bool tcr_all_env_vars_set(const json_t *provider) {
    json_t *env_vars = json_obj_get((json_t *)provider, "env_vars");
    if (!env_vars || env_vars->type != JSON_ARRAY) return true;
    size_t n = json_len(env_vars);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(env_vars, (int)i);
        const char *k = e ? json_get_str(e, "key", NULL) : NULL;
        if (!k) continue;
        char *v = tcr_get_env(k);
        bool present = v && v[0];
        free(v);
        if (!present) return false;
    }
    return true;
}

static bool cat_has(const json_t *obj, const char *key);

static int tcr_prompt_choice(const char *question, char **choices,
                             int n_choices, int def) {
    return curses_radiolist(question, choices, n_choices, def, def, NULL, 1);
}

/* sorted-unique string set */
typedef struct { char **v; size_t n, cap; } tcr_set_t;

static void tcr_set_add(tcr_set_t *s, const char *x) {
    if (!x) return;
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->v[i], x) == 0) return;
    if (s->n == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 16;
        char **nv = (char **)realloc(s->v, nc * sizeof *nv);
        if (!nv) return;
        s->v = nv; s->cap = nc;
    }
    s->v[s->n++] = tcr_strdup(x);
}

static void tcr_save(json_t *config) {
    char *ser = config ? json_dumps(config, 0) : NULL;
    if (ser) { config_save_config(ser); free(ser); }
}

static json_t *tcr_setdefault_section(json_t *config, const char *key) {
    json_t *sec = json_obj_get(config, key);
    if (!(sec && sec->type == JSON_OBJECT)) {
        sec = json_object();
        json_set(config, key, sec);
    }
    return sec;
}

/* ── reconfigure provider ──────────────────────────────────────────────── */

/* PoP: tools_config_reconfigure_provider @ hermes_cli/tools_config.py:_reconfigure_provider */
void tools_config_reconfigure_provider(const json_t *provider, json_t *config,
                                       bool force_fresh) {
    if (!provider || !config) return;
    json_t *env_vars = json_obj_get((json_t *)provider, "env_vars");
    size_t n_env = (env_vars && env_vars->type == JSON_ARRAY)
                       ? json_len(env_vars) : 0;
    const char *managed = tcr_prov_str(provider, "managed_nous_feature");

    /* Same inline Nous Portal gate as _configure_provider. */
    if (managed) {
        json_t *features = tools_config_nous_subscription_features(config, force_fresh);
        json_t *acct = features ? json_obj_get(features, "account_info") : NULL;
        bool ok = acct && tcr_bool_or(acct, "logged_in", false) &&
                  tcr_bool_or(acct, "tool_gateway_entitled", false);
        json_free(features);
        if (!ok) {
            display_print_warning(
                "  Not enabled — Nous Portal access is required for this backend.");
            return;
        }
    } else if (tcr_bool_or(provider, "requires_nous_auth", false)) {
        json_t *features = tools_config_nous_subscription_features(config, force_fresh);
        bool auth_present =
            features ? tcr_bool_or(features, "nous_auth_present", false) : false;
        json_t *acct = features ? json_obj_get(features, "account_info") : NULL;
        bool entitled = acct ? tcr_bool_or(acct, "paid_service_access", false)
                             : false;
        json_free(features);
        if (!auth_present || !entitled) {
            display_print_warning(
                "  Nous Subscription is only available after logging into Nous Portal.");
            return;
        }
    }

    /* Selection writes: one provider string per category + use_gateway drop. */
    const char *tts = tcr_prov_str(provider, "tts_provider");
    if (tts) {
        json_t *sec = tcr_setdefault_section(config, "tts");
        json_set(sec, "provider",
                 json_string(managed ? "nous" : tts));
        json_obj_del(sec, "use_gateway");
        char msg[256];
        snprintf(msg, sizeof msg, "  TTS provider set to: %s", tts);
        display_print_success(msg);
    }
    const char *stt = tcr_prov_str(provider, "stt_provider");
    if (stt) {
        json_t *sec = tcr_setdefault_section(config, "stt");
        json_set(sec, "provider", json_string(managed ? "nous" : stt));
        json_obj_del(sec, "use_gateway");
        char msg[256];
        snprintf(msg, sizeof msg, "  STT provider set to: %s", stt);
        display_print_success(msg);
    }
    if (tcr_has_key(provider, "browser_provider")) {
        const char *bp = tcr_prov_str(provider, "browser_provider");
        json_t *browser_cfg = tcr_setdefault_section(config, "browser");
        if (managed) {
            json_set(browser_cfg, "cloud_provider", json_string("nous"));
            char msg[256];
            snprintf(msg, sizeof msg, "  Browser cloud provider set to: %s",
                     (bp && bp[0]) ? bp : "nous");
            display_print_success(msg);
        } else if (bp && strcmp(bp, "local") == 0) {
            json_set(browser_cfg, "cloud_provider", json_string("local"));
            display_print_success("  Browser set to local mode");
        } else if (bp && bp[0]) {
            json_set(browser_cfg, "cloud_provider", json_string(bp));
            char msg[256];
            snprintf(msg, sizeof msg, "  Browser cloud provider set to: %s", bp);
            display_print_success(msg);
        }
        /* Browser Use mode composes with the provider — keep the driver. */
        json_obj_del(browser_cfg, "use_gateway");
    }
    const char *bb = tcr_prov_str(provider, "browser_backend");
    if (bb) {
        json_t *browser_cfg = tcr_setdefault_section(config, "browser");
        json_set(browser_cfg, "backend", json_string(bb));
        display_print_success("  Browser set to Browser Use (browser_exec via CLI 3.0)");
    }
    const char *wb = tcr_prov_str(provider, "web_backend");
    if (wb) {
        json_t *web_cfg = tcr_setdefault_section(config, "web");
        json_set(web_cfg, "backend", json_string(managed ? "nous" : wb));
        json_obj_del(web_cfg, "use_gateway");
        const char *tier = tcr_prov_str(provider, "web_tier");
        if (tier && tier[0]) {
            json_t *tiers = json_obj_get(web_cfg, "provider_tier");
            if (!(tiers && tiers->type == JSON_OBJECT)) {
                tiers = json_object();
                json_set(web_cfg, "provider_tier", tiers);
            }
            json_set(tiers, wb, json_string(tier));
            char msg[320];
            snprintf(msg, sizeof msg, "  Web backend set to: %s (%s tier)", wb,
                     tier);
            display_print_success(msg);
        } else {
            json_t *stale = json_obj_get(web_cfg, "provider_tier");
            if (stale && stale->type == JSON_OBJECT) json_obj_del(stale, wb);
            char msg[256];
            snprintf(msg, sizeof msg, "  Web backend set to: %s", wb);
            display_print_success(msg);
        }
    }
    const char *cu = tcr_prov_str(provider, "computer_use_backend");
    if (cu) {
        json_t *cu_cfg = tcr_setdefault_section(config, "computer_use");
        json_set(cu_cfg, "backend", json_string(cu));
        char msg[256];
        snprintf(msg, sizeof msg, "  Computer Use backend set to: %s", cu);
        display_print_success(msg);
    }

    if (managed && strcmp(managed, "web") != 0 && strcmp(managed, "tts") != 0 &&
        strcmp(managed, "stt") != 0 && strcmp(managed, "browser") != 0) {
        json_t *section = json_obj_get(config, managed);
        if (!(section && section->type == JSON_OBJECT))
            section = tcr_setdefault_section(config, managed);
        json_set(section, "provider", json_string("nous"));
        json_obj_del(section, "use_gateway");
    } else if (!managed) {
        /* Category walk: clear stale use_gateway on the matching category. */
        extern json_t *tools_config_tool_categories(void);
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
                        if (!p) continue;
                        const char *pn2 = json_get_str(p, "name", "");
                        const char *pn1 = json_get_str((json_t *)provider, "name", "");
                        if (pn1 && pn2 && strcmp(pn1, pn2) == 0) { found = true; break; }
                    }
                }
                if (found) {
                    const char *cat_key = json_object_get_key_at(cats, i);
                    json_t *section = cat_key ? json_obj_get(config, cat_key) : NULL;
                    if (section && section->type == JSON_OBJECT)
                        json_obj_del(section, "use_gateway");
                    break;
                }
            }
        }
    }

    if (n_env == 0) {
        const char *post_setup = tcr_prov_str(provider, "post_setup");
        if (post_setup) tools_config_run_post_setup(post_setup);
        char msg[320];
        snprintf(msg, sizeof msg, "  %s - no configuration needed!",
                 json_get_str(provider, "name", ""));
        display_print_success(msg);
        if (managed)
            display_print_info(
                "  Requests for this tool will be billed to your Nous subscription.");
        const char *plugin_name = tcr_prov_str(provider, "image_gen_plugin_name");
        if (plugin_name) {
            tools_config_select_plugin_image_gen_provider(plugin_name, config,
                                                          managed != NULL);
            return;
        }
        const char *video_plugin = tcr_prov_str(provider, "video_gen_plugin_name");
        if (video_plugin) {
            tools_config_select_plugin_video_gen_provider(video_plugin, config,
                                                          managed != NULL);
            return;
        }
        const char *backend = tcr_prov_str(provider, "imagegen_backend");
        if (backend) {
            tools_config_configure_imagegen_model(backend, config);
            if (strcmp(backend, "fal") == 0) {
                json_t *img = json_obj_get(config, "image_gen");
                if (img && img->type == JSON_OBJECT) {
                    json_set(img, "provider",
                             json_string(managed ? "nous" : "fal"));
                    json_obj_del(img, "use_gateway");
                }
            }
        }
        if (stt && !managed) tools_config_configure_stt_model(stt, config);
        return;
    }

    for (size_t i = 0; i < n_env; i++) {
        json_t *var = json_get(env_vars, (int)i);
        const char *key = var ? json_get_str(var, "key", NULL) : NULL;
        if (!key) continue;
        char *existing = tcr_get_env(key);
        if (existing && existing[0]) {
            char shown[9];
            snprintf(shown, sizeof shown, "%.8s", existing);
            char msg[384];
            snprintf(msg, sizeof msg, "  %s: configured (%s...)", key, shown);
            display_print_info(msg);
        }
        free(existing);
        const char *url = var ? json_get_str(var, "url", "") : "";
        if (url && url[0]) {
            char msg[384];
            snprintf(msg, sizeof msg, "  Get yours at: %s", url);
            display_print_info(msg);
        }
        const char *prompt_text = var ? json_get_str(var, "prompt", key) : key;
        char q[560];
        snprintf(q, sizeof q, "    %s (Enter to keep current)",
                 prompt_text ? prompt_text : key);
        char *value = stp_prompt(q, NULL);
        if (value && value[strspn(value, " \t")]) {
            config_save_env_value(key, value);
            display_print_success("    Updated");
        } else {
            display_print_info("    Kept current");
        }
        free(value);
    }

    const char *post_setup = tcr_prov_str(provider, "post_setup");
    if (post_setup) tools_config_run_post_setup(post_setup);

    const char *plugin_name = tcr_prov_str(provider, "image_gen_plugin_name");
    if (plugin_name) {
        tools_config_select_plugin_image_gen_provider(plugin_name, config,
                                                      managed != NULL);
        return;
    }
    const char *video_plugin = tcr_prov_str(provider, "video_gen_plugin_name");
    if (video_plugin) {
        tools_config_select_plugin_video_gen_provider(video_plugin, config,
                                                      managed != NULL);
        return;
    }
    const char *backend = tcr_prov_str(provider, "imagegen_backend");
    if (backend) {
        tools_config_configure_imagegen_model(backend, config);
        if (strcmp(backend, "fal") == 0) {
            json_t *img = json_obj_get(config, "image_gen");
            if (img && img->type == JSON_OBJECT) {
                json_set(img, "provider",
                         json_string(managed ? "nous" : "fal"));
                json_obj_del(img, "use_gateway");
            }
        }
    }
    if (stt && !managed) tools_config_configure_stt_model(stt, config);
}

/* PoP: tools_config_configure_tool_category_for_reconfig @ hermes_cli/tools_config.py:_configure_tool_category_for_reconfig */
void tools_config_configure_tool_category_for_reconfig(const char *ts_key,
                                                       const json_t *cat,
                                                       json_t *config,
                                                       bool force_fresh) {
    (void)ts_key;
    if (!cat || !config) return;
    const char *icon = tcp_helper_cat_str(cat, "icon", "");
    const char *name = tcp_helper_cat_str(cat, "name", "");
    json_t *providers = tools_config_visible_providers(cat, config, force_fresh, NULL);
    size_t n = json_len(providers);

    if (n == 1) {
        json_t *provider = json_get(providers, 0);
        printf("\n");
        char header[640];
        snprintf(header, sizeof header, "  --- %s %s (%s) ---", icon, name,
                 json_get_str(provider, "name", ""));
        printf("%s\n", header);
        tools_config_reconfigure_provider(provider, config, force_fresh);
        json_free(providers);
        return;
    }

    printf("\n");
    char header[768];
    snprintf(header, sizeof header, "  --- %s %s - Choose a provider ---",
             icon, name);
    printf("%s\n", header);
    printf("\n");

    size_t total = n;
    char **choices = (char **)calloc(total + 1, sizeof *choices);
    if (!choices) { json_free(providers); return; }
    for (size_t i = 0; i < n; i++) {
        json_t *p = json_get(providers, (int)i);
        const char *badge = json_get_str(p, "badge", "");
        const char *tag = json_get_str(p, "tag", "");
        const char *configured = "";
        if (tcr_all_env_vars_set(p)) {
            if (tools_config_is_provider_active(p, config, force_fresh, NULL))
                configured = " [active]";
            else if (!tcp_helper_has_env_vars(p))
                configured = "";
            else
                configured = " [configured]";
        }
        choices[i] = (char *)malloc(1024);
        snprintf(choices[i], 1024, "%s%s%s%s%s%s%s",
                 json_get_str(p, "name", ""),
                 (badge && badge[0]) ? " [" : "",
                 (badge && badge[0]) ? badge : "",
                 (badge && badge[0]) ? "]" : "",
                 (tag && tag[0]) ? " — " : "",
                 (tag && tag[0]) ? tag : "",
                 configured);
    }

    /* _detect_active_provider_index analog: active row first, then any row
     * whose env vars are all present. */
    int default_idx = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *p = json_get(providers, (int)i);
        if (tools_config_is_provider_active(p, config, force_fresh, NULL)) {
            default_idx = (int)i; break;
        }
        if (tcp_helper_has_env_vars(p) && tcr_all_env_vars_set(p)) {
            default_idx = (int)i; break;
        }
    }

    int idx = tcr_prompt_choice("  Select provider:", choices, (int)n,
                                default_idx);
    for (size_t i = 0; i < total; i++) free(choices[i]);
    free(choices);
    if (idx < 0 || idx >= (int)n) idx = 0;
    tools_config_reconfigure_provider(json_get(providers, idx), config,
                                      force_fresh);
    json_free(providers);
}

/* PoP: tools_config_reconfigure_simple_requirements @ hermes_cli/tools_config.py:_reconfigure_simple_requirements */
void tools_config_reconfigure_simple_requirements(const char *ts_key) {
    if (!ts_key) return;
    if (strcmp(ts_key, "vision") == 0) {
        /* Vision has its own provider/model picker — run it directly so
         * reconfigure doesn't fall back to the generic single-key prompt. */
        tools_config_configure_vision_backend();
        return;
    }
    json_t *reqs_doc = tools_config_toolset_env_requirements();
    json_t *requirements = reqs_doc ? json_obj_get(reqs_doc, ts_key) : NULL;
    if (!requirements || requirements->type != JSON_ARRAY ||
        json_len(requirements) == 0)
        return;

    /* ts_label from the effective configurable toolsets. */
    const char *ts_label = ts_key;
    size_t rn = 0;
    tools_config_toolset_row_t *rows =
        tools_config_get_effective_configurable_toolsets(&rn);
    for (size_t i = 0; i < rn; i++)
        if (strcmp(rows[i].key, ts_key) == 0) { ts_label = rows[i].label; break; }

    printf("\n");
    char header[640];
    snprintf(header, sizeof header, "  %s:", ts_label);
    printf("%s\n", header);

    size_t n = json_len(requirements);
    for (size_t i = 0; i < n; i++) {
        json_t *pair = json_get(requirements, (int)i);
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 1) continue;
        const char *var = json_string_value(json_get(pair, 0));
        const char *url = json_len(pair) > 1
                              ? json_string_value(json_get(pair, 1)) : NULL;
        if (!var) continue;
        char *existing = tcr_get_env(var);
        if (existing && existing[0]) {
            char shown[9];
            snprintf(shown, sizeof shown, "%.8s", existing);
            char msg[384];
            snprintf(msg, sizeof msg, "  %s: configured (%s...)", var, shown);
            display_print_info(msg);
        }
        free(existing);
        if (url && url[0]) {
            char msg[384];
            snprintf(msg, sizeof msg, "  Get key at: %s", url);
            display_print_info(msg);
        }
        char q[300];
        snprintf(q, sizeof q, "    %s (Enter to keep current)", var);
        char *value = stp_prompt(q, NULL);
        if (value && value[strspn(value, " \t")]) {
            config_save_env_value(var, value);
            display_print_success("    Updated");
        } else {
            display_print_info("    Kept current");
        }
        free(value);
    }
}

/* PoP: tools_config_reconfigure_tool @ hermes_cli/tools_config.py:_reconfigure_tool */
void tools_config_reconfigure_tool(json_t *config, bool force_fresh) {
    /* Configurable tools that are currently set up. */
    size_t all_n = 0;
    tools_config_toolset_row_t *effective =
        tools_config_get_effective_configurable_toolsets(&all_n);

    tcr_set_t keys = { 0 }, labels = { 0 };
    for (size_t i = 0; i < all_n; i++) {
        const char *k = effective[i].key;
        extern json_t *tools_config_tool_categories(void);
        extern json_t *tools_config_toolset_env_requirements(void);
        json_t *cats = tools_config_tool_categories();
        json_t *reqs_doc = tools_config_toolset_env_requirements();
        bool has_cat = cats && cat_has(cats, k);
        bool has_reqs = reqs_doc && cat_has(reqs_doc, k);
        if (!(has_cat || has_reqs)) continue;
        if (tools_config_toolset_has_keys(k, config, force_fresh) ||
            tools_config_toolset_enabled_for_reconfigure(k, config)) {
            tcr_set_add(&keys, k);
            tcr_set_add(&labels, effective[i].label);
        }
    }

    if (keys.n == 0) {
        display_print_info("No configured tools to reconfigure.");
        tools_config_free_toolset_rows(effective, all_n);
        return;
    }

    size_t n_choices = keys.n + 1;
    char **choices = (char **)calloc(n_choices + 1, sizeof *choices);
    for (size_t i = 0; i < keys.n; i++) choices[i] = labels.v[i];
    choices[keys.n] = tcr_strdup("Cancel");

    int idx = tcr_prompt_choice(
        "  Which tool would you like to reconfigure?", choices,
        (int)n_choices, (int)n_choices - 1);

    for (size_t i = 0; i < keys.n; i++) free(labels.v[i]);
    free(labels.v);
    free(choices);

    if (idx >= 0 && idx < (int)keys.n) {
        const char *ts_key = keys.v[idx];
        extern json_t *tools_config_tool_categories(void);
        json_t *cats = tools_config_tool_categories();
        json_t *cat = cats ? json_obj_get(cats, ts_key) : NULL;
        if (cat && cat->type == JSON_OBJECT)
            tools_config_configure_tool_category_for_reconfig(ts_key, cat,
                                                              config,
                                                              force_fresh);
        else
            tools_config_reconfigure_simple_requirements(ts_key);
    }
    for (size_t i = 0; i < keys.n; i++) free(keys.v[i]);
    free(keys.v);
    tools_config_free_toolset_rows(effective, all_n);
    tcr_save(config);
}

/* helper: object contains key */
static bool cat_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get((json_t *)obj, key);
}

/* ── MCP interactive configuration ─────────────────────────────────────── */

/* PoP: tools_config_configure_mcp_tools_interactive @ hermes_cli/tools_config.py:_configure_mcp_tools_interactive */
void tools_config_configure_mcp_tools_interactive(json_t *config) {
    json_t *mcp_servers = config ? json_obj_get(config, "mcp_servers") : NULL;
    if (!mcp_servers || mcp_servers->type != JSON_OBJECT ||
        json_object_size(mcp_servers) == 0) {
        display_print_info("No MCP servers configured.");
        return;
    }

    size_t sn = json_object_size(mcp_servers);
    size_t enabled_count = 0;
    for (size_t i = 0; i < sn; i++) {
        json_t *v = json_object_get_at(mcp_servers, i);
        json_t *en = v ? json_obj_get(v, "enabled") : NULL;
        bool enabled = true;
        if (en) {
            if (en->type == JSON_BOOL) enabled = en->bool_val;
            else if (en->type == JSON_STRING && en->str_val)
                enabled = !(strcmp(en->str_val, "false") == 0 ||
                            strcmp(en->str_val, "0") == 0 ||
                            strcmp(en->str_val, "no") == 0 ||
                            strcmp(en->str_val, "off") == 0);
        }
        if (enabled) enabled_count++;
    }
    if (enabled_count == 0) {
        display_print_info("All MCP servers are disabled.");
        return;
    }

    display_print_warning("  Discovering tools from MCP servers...");

    /* probe_mcp_server_tools() has no C port yet — the discovery step is the
     * network boundary owned by the MCP runtime. Mirror its empty-result
     * branch faithfully so the flow degrades the same way. */
    display_print_error("Failed to probe MCP servers.");
    return;
}

/* PoP: tools_config_apply_mcp_change @ hermes_cli/tools_config.py:_apply_mcp_change */
json_t *tools_config_apply_mcp_change(json_t *config,
                                      const char *const *targets,
                                      size_t n_targets, const char *action) {
    json_t *failed = json_array();
    json_t *mcp_servers = config ? json_obj_get(config, "mcp_servers") : NULL;

    for (size_t t = 0; t < n_targets; t++) {
        const char *target = targets[t];
        if (!target) continue;
        const char *colon = strchr(target, ':');
        if (!colon) continue;   /* malformed target: Python split(":",1) needs it */
        size_t srv_len = (size_t)(colon - target);
        char server_name[256];
        snprintf(server_name, sizeof server_name, "%.*s", (int)srv_len, target);
        const char *tool_name = colon + 1;

        json_t *srv_cfg = (mcp_servers && mcp_servers->type == JSON_OBJECT)
                              ? json_obj_get(mcp_servers, server_name) : NULL;
        if (!srv_cfg) { json_append(failed, json_string(server_name)); continue; }

        json_t *tools_cfg = json_obj_get(srv_cfg, "tools");
        if (!(tools_cfg && tools_cfg->type == JSON_OBJECT)) {
            tools_cfg = json_object();
            json_set(srv_cfg, "tools", tools_cfg);
        }
        json_t *exclude = json_obj_get(tools_cfg, "exclude");
        if (!(exclude && exclude->type == JSON_ARRAY))
            exclude = json_array();

        bool present = false;
        size_t xn = json_len(exclude);
        for (size_t i = 0; i < xn; i++) {
            const char *e = json_string_value(json_get(exclude, (int)i));
            if (e && strcmp(e, tool_name) == 0) { present = true; break; }
        }
        if (action && strcmp(action, "disable") == 0) {
            if (!present) json_append(exclude, json_string(tool_name));
        } else if (present) {
            for (size_t i = 0; i < xn; i++) {
                const char *e = json_string_value(json_get(exclude, (int)i));
                if (e && strcmp(e, tool_name) == 0) {
                    json_array_remove(exclude, i);
                    break;
                }
            }
        }
        json_set(tools_cfg, "exclude", exclude);
    }
    return failed;
}

/* ── tools listing + non-interactive enable/disable ───────────────────── */

static void tcr_print_status(bool enabled, const char *ts_key,
                             const char *label) {
    printf("  %s  %s  \x1b[2m%s\x1b[0m\n",
           enabled ? "\x1b[32m✓ enabled\x1b[0m" : "\x1b[31m✗ disabled\x1b[0m",
           ts_key, label);
}

/* PoP: tools_config_print_tools_list @ hermes_cli/tools_config.py:_print_tools_list */
void tools_config_print_tools_list(const json_t *enabled_toolsets,
                                   const json_t *mcp_servers,
                                   const char *platform) {
    if (!platform) platform = "cli";
    size_t all_n = 0;
    tools_config_toolset_row_t *effective_all =
        tools_config_get_effective_configurable_toolsets(&all_n);

    /* builtin_keys = CONFIGURABLE_TOOLSETS keys */
    printf("Built-in toolsets (%s):\n", platform);
    for (size_t i = 0; i < all_n; i++) {
        const tools_config_toolset_row_t *b;
        bool builtin = false;
        for (b = tools_config_configurable_toolsets(); b->key; b++)
            if (strcmp(b->key, effective_all[i].key) == 0) { builtin = true; break; }
        if (!builtin) continue;
        if (!toolset_allowed_for_platform(effective_all[i].key, platform)) continue;
        bool enabled = false;
        if (enabled_toolsets && enabled_toolsets->type == JSON_ARRAY) {
            size_t n = json_len(enabled_toolsets);
            for (size_t j = 0; j < n; j++)
                if (strcmp(json_string_value(json_get(enabled_toolsets, (int)j)),
                           effective_all[i].key) == 0) { enabled = true; break; }
        }
        tcr_print_status(enabled, effective_all[i].key, effective_all[i].label);
    }

    printf("\nPlugin toolsets (%s):\n", platform);
    for (size_t i = 0; i < all_n; i++) {
        const tools_config_toolset_row_t *b;
        bool builtin = false;
        for (b = tools_config_configurable_toolsets(); b->key; b++)
            if (strcmp(b->key, effective_all[i].key) == 0) { builtin = true; break; }
        if (builtin) continue;
        if (!toolset_allowed_for_platform(effective_all[i].key, platform)) continue;
        bool enabled = false;
        if (enabled_toolsets && enabled_toolsets->type == JSON_ARRAY) {
            size_t n = json_len(enabled_toolsets);
            for (size_t j = 0; j < n; j++)
                if (strcmp(json_string_value(json_get(enabled_toolsets, (int)j)),
                           effective_all[i].key) == 0) { enabled = true; break; }
        }
        tcr_print_status(enabled, effective_all[i].key, effective_all[i].label);
    }

    if (mcp_servers && mcp_servers->type == JSON_OBJECT &&
        json_object_size(mcp_servers) > 0) {
        printf("\nMCP servers:\n");
        size_t n = json_object_size(mcp_servers);
        for (size_t i = 0; i < n; i++) {
            const char *srv = json_object_get_key_at(mcp_servers, i);
            json_t *cfg = json_object_get_at(mcp_servers, i);
            json_t *tools_cfg = cfg ? json_obj_get(cfg, "tools") : NULL;
            json_t *exclude = tools_cfg ? json_obj_get(tools_cfg, "exclude") : NULL;
            json_t *include = tools_cfg ? json_obj_get(tools_cfg, "include") : NULL;
            if (include && include->type == JSON_ARRAY && json_len(include) > 0) {
                char list[2048] = "";
                size_t in_n = json_len(include);
                for (size_t j = 0; j < in_n; j++) {
                    strncat(list, json_string_value(json_get(include, (int)j)) ?: "",
                            sizeof list - strlen(list) - 2);
                    if (j + 1 < in_n) strncat(list, ", ", sizeof list - strlen(list) - 1);
                }
                char msg[2304];
                snprintf(msg, sizeof msg, "%s  [include only: %s]", srv ? srv : "", list);
                display_print_info(msg);
            } else if (exclude && exclude->type == JSON_ARRAY && json_len(exclude) > 0) {
                char list[2048] = "";
                size_t ex_n = json_len(exclude);
                for (size_t j = 0; j < ex_n; j++) {
                    strncat(list, json_string_value(json_get(exclude, (int)j)) ?: "",
                            sizeof list - strlen(list) - 2);
                    if (j + 1 < ex_n) strncat(list, ", ", sizeof list - strlen(list) - 1);
                }
                char msg[2304];
                snprintf(msg, sizeof msg, "%s  [excluded: %s]", srv ? srv : "", list);
                display_print_info(msg);
            } else {
                char msg[512];
                snprintf(msg, sizeof msg, "%s  all tools enabled", srv ? srv : "");
                display_print_info(msg);
            }
        }
    }
    tools_config_free_toolset_rows(effective_all, all_n);
}

/* PoP: tools_config_disable_enable_command @ hermes_cli/tools_config.py:tools_disable_enable_command */
void tools_config_disable_enable_command(const char *action,
                                         const char *platform,
                                         const char *const *names,
                                         size_t n_names) {
    if (!action) return;
    if (!platform) platform = "cli";
    json_t *config = config_py_load_config_impl(1);
    if (!config) return;

    extern char **tools_config_known_tool_platforms(size_t *out_n);
    size_t vn = 0;
    char **valid_platforms = tools_config_known_tool_platforms(&vn);
    bool platform_ok = false;
    for (size_t i = 0; i < vn; i++)
        if (strcmp(valid_platforms[i], platform) == 0) { platform_ok = true; break; }
    tools_config_free_list(valid_platforms, vn);
    if (!platform_ok) {
        char msg[256];
        snprintf(msg, sizeof msg, "Unknown platform '%s'.", platform);
        display_print_error(msg);
        json_free(config);
        return;
    }

    if (strcmp(action, "list") == 0) {
        json_t *enabled = platform_tools_get(config, platform, false, NULL);
        json_t *mcp = json_obj_get(config, "mcp_servers");
        tools_config_print_tools_list(enabled, mcp, platform);
        json_free(enabled);
        json_free(config);
        return;
    }

    /* Split targets into toolset vs MCP (server:tool). */
    tcr_set_t toolset_targets = { 0 }, mcp_targets = { 0 };
    tcr_set_t unknown = { 0 }, restricted = { 0 };
    for (size_t i = 0; i < n_names; i++) {
        const char *t = names[i];
        if (!t || !*t) continue;
        if (strchr(t, ':')) { tcr_set_add(&mcp_targets, t); continue; }
        tcr_set_add(&toolset_targets, t);
    }

    /* Validate toolsets. */
    size_t pn = 0;
    char **plugin_keys = tools_config_get_plugin_toolset_keys(&pn);
    for (size_t i = 0; i < toolset_targets.n; i++) {
        const char *k = toolset_targets.v[i];
        bool valid = false;
        for (const tools_config_toolset_row_t *b =
                 tools_config_configurable_toolsets(); b->key; b++)
            if (strcmp(b->key, k) == 0) { valid = true; break; }
        for (size_t p = 0; !valid && p < pn; p++)
            if (strcmp(plugin_keys[p], k) == 0) { valid = true; break; }
        if (!valid) {
                tcr_set_add(&unknown, k);
        } else if (!toolset_allowed_for_platform(k, platform)) {
            char msg[384];
            snprintf(msg, sizeof msg,
                     "Toolset '%s' is not available on platform '%s'.", k,
                     platform);
            display_print_error(msg);
            tcr_set_add(&restricted, k);
        }
    }
    tools_config_free_list(plugin_keys, pn);

    /* Drop invalid targets from the working sets. */
    for (size_t i = 0; i < unknown.n; i++) {
        for (size_t j = 0; j < toolset_targets.n;) {
            if (strcmp(toolset_targets.v[j], unknown.v[i]) == 0) {
                free(toolset_targets.v[j]);
                toolset_targets.v[j] = toolset_targets.v[--toolset_targets.n];
            } else j++;
        }
    }
    for (size_t i = 0; i < restricted.n; i++) {
        for (size_t j = 0; j < toolset_targets.n;) {
            if (strcmp(toolset_targets.v[j], restricted.v[i]) == 0) {
                free(toolset_targets.v[j]);
                toolset_targets.v[j] = toolset_targets.v[--toolset_targets.n];
            } else j++;
        }
    }

    /* Apply toolset change via the shared add/remove path. */
    if (toolset_targets.n > 0) {
        json_t *enabled = platform_tools_get(config, platform, false, NULL);
        for (size_t i = 0; i < toolset_targets.n; i++) {
            const char *k = toolset_targets.v[i];
            bool present = false;
            size_t en = json_len(enabled);
            for (size_t j = 0; j < en; j++)
                if (strcmp(json_string_value(json_get(enabled, (int)j)), k) == 0) {
                    present = true; break;
                }
            if (strcmp(action, "disable") == 0) {
                if (present)
                    for (size_t j = 0; j < en; j++) {
                        if (strcmp(json_string_value(json_get(enabled, (int)j)), k) == 0) {
                            json_array_remove(enabled, j);
                            break;
                        }
                    }
            } else if (!present) {
                json_append(enabled, json_string(k));
            }
        }
        /* platform_tools_save takes a set of keys. */
        size_t sn = json_len(enabled);
        const char **ksave = (const char **)calloc(sn + 1, sizeof *ksave);
        for (size_t j = 0; j < sn; j++)
            ksave[j] = json_string_value(json_get(enabled, (int)j));
        platform_tools_save(config, platform, ksave, sn);
        free(ksave);
        json_free(enabled);
    }

    /* Apply MCP changes and report failures. */
    if (mcp_targets.n > 0) {
        json_t *failed = tools_config_apply_mcp_change(
            config, (const char *const *)mcp_targets.v, mcp_targets.n, action);
        size_t fn = json_len(failed);
        for (size_t i = 0; i < fn; i++) {
            const char *srv = json_string_value(json_get(failed, (int)i));
            char msg[320];
            snprintf(msg, sizeof msg, "MCP server '%s' not found in config",
                     srv ? srv : "");
            display_print_error(msg);
        }
        json_free(failed);
    }

    tcr_save(config);

    /* Success summary over the surviving targets. */
    tcr_set_t successful = { 0 };
    for (size_t i = 0; i < n_names; i++) {
        const char *t = names[i];
        if (!t || !*t) continue;
        bool bad = false;
        for (size_t u = 0; u < unknown.n; u++) if (strcmp(unknown.v[u], t) == 0) bad = true;
        for (size_t r = 0; r < restricted.n; r++) if (strcmp(restricted.v[r], t) == 0) bad = true;
        if (bad) continue;
        if (strchr(t, ':')) {
            char srv[256] = "";
            const char *colon = strchr(t, ':');
            snprintf(srv, sizeof srv, "%.*s", (int)(colon - t), t);
            bool failed_srv = false;
            /* re-check against config presence */
            json_t *mcp = json_obj_get(config, "mcp_servers");
            if (!(mcp && mcp->type == JSON_OBJECT &&
                  json_obj_get(mcp, srv)))
                failed_srv = true;
            if (failed_srv) continue;
        }
        tcr_set_add(&successful, t);
    }
    if (successful.n > 0) {
        char joined[2048] = "";
        for (size_t i = 0; i < successful.n; i++) {
            strncat(joined, successful.v[i], sizeof joined - strlen(joined) - 2);
            if (i + 1 < successful.n)
                strncat(joined, ", ", sizeof joined - strlen(joined) - 1);
        }
        char msg[2200];
        snprintf(msg, sizeof msg, "%s: %s",
                 strcmp(action, "disable") == 0 ? "Disabled" : "Enabled", joined);
        display_print_success(msg);
    }
    for (size_t i = 0; i < successful.n; i++) free(successful.v[i]);
    free(successful.v);
    for (size_t i = 0; i < toolset_targets.n; i++) free(toolset_targets.v[i]);
    free(toolset_targets.v);
    for (size_t i = 0; i < mcp_targets.n; i++) free(mcp_targets.v[i]);
    free(mcp_targets.v);
    for (size_t i = 0; i < unknown.n; i++) free(unknown.v[i]);
    free(unknown.v);
    for (size_t i = 0; i < restricted.n; i++) free(restricted.v[i]);
    free(restricted.v);
    json_free(config);
}
