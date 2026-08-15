/* port_platform_tools.c — faithful C11 port of the platform-toolset
 * resolution surface of hermes_cli/tools_config.py. Reuses port_toolsets.c
 * (toolsets.py), parse_enabled_flag + toolset_allowed_for_platform
 * (port_tools_config_helpers.c) and config_py_get_env_value_prefer_dotenv.
 */
#define _GNU_SOURCE
#include "platform_tools.h"
#include "toolsets.h"
#include "hermes_auth.h" /* auth_entry_t for xai_credentials_present */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* existing plumbing (grep-verified) */
extern bool parse_enabled_flag(const char *value, bool default_val);
extern bool toolset_allowed_for_platform(const char *ts_key, const char *platform);
extern char *config_py_get_env_value_prefer_dotenv(const char *key);

/* PoP: configurable_toolsets_keys @ hermes_cli/tools_config.py:CONFIGURABLE_TOOLSETS */
static const char *const CONFIGURABLE_KEYS[] = {
    "web", "browser", "terminal", "file", "code_execution", "vision",
    "video", "image_gen", "video_gen", "x_search", "tts", "skills", "todo",
    "memory", "context_engine", "session_search", "clarify", "delegation",
    "cronjob", "homeassistant", "spotify", "discord", "discord_admin",
    "yuanbao", "computer_use", NULL
};

/* PoP: default_off_toolsets @ hermes_cli/tools_config.py:_DEFAULT_OFF_TOOLSETS */
static const char *const DEFAULT_OFF[] = {
    "homeassistant", "spotify", "discord", "discord_admin", "video",
    "video_gen", "x_search", NULL
};

/* PLATFORMS registry (hermes_cli/platforms.py) */
typedef struct { const char *key; const char *default_toolset; } plat_row_t;
static const plat_row_t PLATFORM_ROWS[] = {
    { "cli", "hermes-cli" },
    { "telegram", "hermes-telegram" },
    { "discord", "hermes-discord" },
    { "slack", "hermes-slack" },
    { "whatsapp", "hermes-whatsapp" },
    { "whatsapp_cloud", "hermes-whatsapp" },
    { "signal", "hermes-signal" },
    { "bluebubbles", "hermes-bluebubbles" },
    { "email", "hermes-email" },
    { "homeassistant", "hermes-homeassistant" },
    { "mattermost", "hermes-mattermost" },
    { "matrix", "hermes-matrix" },
    { "dingtalk", "hermes-dingtalk" },
    { "feishu", "hermes-feishu" },
    { "wecom", "hermes-wecom" },
    { "wecom_callback", "hermes-wecom-callback" },
    { "weixin", "hermes-weixin" },
    { "qqbot", "hermes-qqbot" },
    { "yuanbao", "hermes-yuanbao" },
    { "webhook", "hermes-webhook" },
    { "api_server", "hermes-api-server" },
    { "cron", "hermes-cron" },
    { NULL, NULL }
};

const char *const *platform_tools_configurable_keys(void) { return CONFIGURABLE_KEYS; }
const char *const *platform_tools_default_off(void) { return DEFAULT_OFF; }

const char *platform_tools_default_toolset(const char *platform) {
    if (!platform) return NULL;
    for (size_t i = 0; PLATFORM_ROWS[i].key; i++)
        if (strcmp(PLATFORM_ROWS[i].key, platform) == 0)
            return PLATFORM_ROWS[i].default_toolset;
    return NULL;
}

/* ── string-set helpers (sorted unique, mirrors Python set→sorted) ───── */
typedef struct { char **v; size_t n, cap; } pset_t;
static void ps_add(pset_t *s, const char *x) {
    if (!x) return;
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->v[i], x) == 0) return;
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->v = realloc(s->v, s->cap * sizeof(char *));
    }
    s->v[s->n++] = strdup(x);
}
static bool ps_has(const pset_t *s, const char *x) {
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->v[i], x) == 0) return true;
    return false;
}
static void ps_del(pset_t *s, const char *x) {
    for (size_t i = 0; i < s->n; i++) {
        if (strcmp(s->v[i], x) == 0) {
            free(s->v[i]);
            memmove(&s->v[i], &s->v[i + 1], (s->n - i - 1) * sizeof(char *));
            s->n--;
            return;
        }
    }
}
static void ps_free(pset_t *s) {
    for (size_t i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v);
    s->v = NULL; s->n = s->cap = 0;
}
static int ps_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static char **ps_finish(pset_t *s, size_t *out_n) {
    qsort(s->v, s->n, sizeof(char *), ps_cmp);
    *out_n = s->n;
    return s->v;
}
/* is `needle` in a NULL-terminated const list? */
static bool in_list(const char *const *list, const char *needle) {
    for (size_t i = 0; list[i]; i++)
        if (strcmp(list[i], needle) == 0) return true;
    return false;
}
/* is set `sub` (char** with n) a subset of pset `sup`? */
static bool subset_of(char **sub, size_t n_sub, const pset_t *sup) {
    for (size_t i = 0; i < n_sub; i++)
        if (!ps_has(sup, sub[i])) return false;
    return true;
}

void platform_tools_free_list(char **list, size_t n) {
    if (!list) return;
    for (size_t i = 0; i < n; i++) free(list[i]);
    free(list);
}

/* stringify a scalar json value for parse_enabled_flag */
static char *json_scalar_to_str(const json_t *v) {
    if (!v) return NULL;
    switch (v->type) {
    case JSON_STRING: return strdup(v->str_val);
    case JSON_BOOL:   return strdup(v->bool_val ? "true" : "false");
    case JSON_NUMBER: {
        char buf[32];
        if (v->num_val == (long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return strdup(buf);
    }
    default: return NULL;
    }
}

/* PoP: platform_tools_enabled_mcp_server_names @ hermes_cli/tools_config.py:enabled_mcp_server_names */
char **platform_tools_enabled_mcp_server_names(const json_t *config,
                                               size_t *out_n) {
    pset_t s = {0};
    const json_t *mcp = config ? json_obj_get(config, "mcp_servers") : NULL;
    if (mcp && mcp->type == JSON_OBJECT) {
        for (size_t i = 0; i < mcp->c.count; i++) {
            const json_t *entry = mcp->c.items[i];
            const char *name = mcp->c.keys[i];
            if (!entry || entry->type != JSON_OBJECT || !name) continue;
            const json_t *en = json_obj_get(entry, "enabled");
            bool enabled = true;
            if (en) {
                char *sv = json_scalar_to_str(en);
                enabled = parse_enabled_flag(sv, true);
                /* Python: bool identity first, then int/str; JSON bool maps
                 * directly */
                if (en->type == JSON_BOOL) enabled = en->bool_val;
                else if (en->type == JSON_NUMBER) enabled = en->num_val != 0;
                free(sv);
            }
            if (enabled) ps_add(&s, name);
        }
    }
    return ps_finish(&s, out_n);
}

/* PoP: platform_tools_get_enabled_platforms @ hermes_cli/tools_config.py:_get_enabled_platforms */
char **platform_tools_get_enabled_platforms(size_t *out_n) {
    pset_t s = {0};
    ps_add(&s, "cli");
    static const struct { const char *env; const char *plat; } CHECKS[] = {
        { "TELEGRAM_BOT_TOKEN", "telegram" },
        { "DISCORD_BOT_TOKEN", "discord" },
        { "SLACK_BOT_TOKEN", "slack" },
        { "WHATSAPP_ENABLED", "whatsapp" },
        { "QQ_APP_ID", "qqbot" },
        { NULL, NULL }
    };
    for (size_t i = 0; CHECKS[i].env; i++) {
        char *v = config_py_get_env_value_prefer_dotenv(CHECKS[i].env);
        if (v && v[0]) ps_add(&s, CHECKS[i].plat);
        free(v);
    }
    /* insertion order in Python is list-append; the summary consumer treats
     * it as a set — keep sorted-unique contract */
    return ps_finish(&s, out_n);
}

/* PoP: platform_tools_configuration_platform @ hermes_cli/tools_config.py:_toolset_configuration_platform */
char *platform_tools_configuration_platform(const char *ts_key,
                                            const char *default_platform) {
    const char *def = default_platform ? default_platform : "cli";
    /* restriction table lives in port_tools_config_helpers.c; mirror the
     * two-entry table here for the sorted()[0] fallback */
    if (ts_key && (strcmp(ts_key, "discord") == 0 ||
                   strcmp(ts_key, "discord_admin") == 0)) {
        if (strcmp(def, "discord") == 0) return strdup(def);
        return strdup("discord"); /* sorted({"discord"})[0] */
    }
    return strdup(def);
}

/* PoP: exempt_explicit_platform_native @ hermes_cli/tools_config.py:_exempt_explicit_platform_native */
static void exempt_explicit_platform_native(pset_t *default_off,
                                            const char *platform,
                                            bool explicitly_configured) {
    if (!explicitly_configured) return;
    /* restricted-and-native toolsets: discord/discord_admin on discord */
    for (size_t i = 0; i < default_off->n;) {
        const char *ts = default_off->v[i];
        bool restricted_here =
            (strcmp(ts, "discord") == 0 || strcmp(ts, "discord_admin") == 0) &&
            strcmp(platform, "discord") == 0;
        if (restricted_here) { ps_del(default_off, ts); }
        else i++;
    }
}

/* PoP: xai_credentials_present @ hermes_cli/tools_config.py:_xai_credentials_present */
static bool xai_credentials_present(void) {
    /* xAI OAuth tokens in the auth store (SuperGrok / Premium+).
     * auth_store_load requires a real home path — passing NULL crashes.
     * Resolve HERMES_HOME (or ~/.hermes) exactly like the Python auth
     * store does; skip the store check when no home can be resolved. */
    extern auth_entry_t *auth_store_load(const char *hermes_home, int *count);
    extern void auth_store_free(auth_entry_t *entries, int count);
    char home_buf[512];
    const char *hh = getenv("HERMES_HOME");
    if (!hh || !hh[0]) {
        const char *h = getenv("HOME");
        if (h && h[0]) {
            snprintf(home_buf, sizeof(home_buf), "%s/.hermes", h);
            hh = home_buf;
        } else {
            hh = NULL;
        }
    }
    if (hh) {
        int n = 0;
        auth_entry_t *entries = auth_store_load(hh, &n);
        bool have_oauth = false;
        for (int i = 0; i < n && entries; i++) {
            if (strncmp(entries[i].provider, "xai", 3) == 0 &&
                entries[i].token.access_token[0]) { have_oauth = true; break; }
        }
        if (entries) auth_store_free(entries, n);
        if (have_oauth) return true;
    }
    /* XAI_API_KEY via .env/config, then raw env */
    char *v = config_py_get_env_value_prefer_dotenv("XAI_API_KEY");
    bool ok = v && v[0];
    free(v);
    if (ok) return true;
    const char *e = getenv("XAI_API_KEY");
    return e && e[0];
}

/* helper: config[platform_toolsets][platform] as list of strdup'd names.
 * *explicit_out = json list present (even empty). */
static char **saved_toolset_names(const json_t *config, const char *platform,
                                  size_t *out_n, bool *explicit_out) {
    *out_n = 0;
    *explicit_out = false;
    const json_t *pt = config ? json_obj_get(config, "platform_toolsets") : NULL;
    const json_t *lst = pt ? json_obj_get(pt, platform) : NULL;
    if (!lst || lst->type != JSON_ARRAY) return NULL;
    *explicit_out = true;
    size_t n = json_len(lst);
    char **v = calloc(n ? n : 1, sizeof(char *));
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *e = json_get(lst, i);
        char *s = json_scalar_to_str(e);
        if (s) v[k++] = s;
    }
    *out_n = k;
    return v;
}

/* Final stage of _get_platform_tools: non-configurable toolset recovery,
 * plugin toolsets, context-engine injection, explicit passthrough, MCP
 * allowlist and agent.disabled_toolsets override. */
static char **platform_tools_get_finish(const json_t *config,
                                        const char *platform,
                                        bool include_default_mcp_servers,
                                        pset_t *enabled,
                                        char **toolset_names, size_t n_names,
                                        bool explicit_empty_selection,
                                        size_t *out_n) {
    /* ── Recover non-configurable platform toolsets ──────────────────── */
    const char *def_ts = platform_tools_default_toolset(platform);
    char fallback[128];
    if (!def_ts) {
        snprintf(fallback, sizeof(fallback), "hermes-%s", platform);
        def_ts = fallback;
    }
    pset_t platform_universe = {0}, configurable_universe = {0}, claimed = {0};
    {
        size_t rn = 0;
        char **rt = toolsets_resolve(def_ts, true, &rn);
        for (size_t i = 0; i < rn; i++) ps_add(&platform_universe, rt[i]);
        toolsets_free_list(rt, rn);
    }
    for (size_t i = 0; CONFIGURABLE_KEYS[i]; i++) {
        size_t rn = 0;
        char **rt = toolsets_resolve(CONFIGURABLE_KEYS[i], true, &rn);
        for (size_t j = 0; j < rn; j++) ps_add(&configurable_universe, rt[j]);
        toolsets_free_list(rt, rn);
    }
    for (size_t i = 0; i < enabled->n; i++) {
        size_t rn = 0;
        char **rt = toolsets_resolve(enabled->v[i], true, &rn);
        for (size_t j = 0; j < rn; j++) ps_add(&claimed, rt[j]);
        toolsets_free_list(rt, rn);
    }
    {
        size_t nn = 0;
        char **all_names = toolsets_get_names(&nn);
        for (size_t i = 0; i < nn; i++) {
            const char *ts_key = all_names[i];
            /* skip: configurable, platform defaults, hermes-*, default-off
             * (minus platform) */
            if (in_list(CONFIGURABLE_KEYS, ts_key)) continue;
            bool is_plat_default = false;
            for (size_t j = 0; PLATFORM_ROWS[j].key; j++)
                if (strcmp(PLATFORM_ROWS[j].default_toolset, ts_key) == 0) { is_plat_default = true; break; }
            if (is_plat_default) continue;
            if (strncmp(ts_key, "hermes-", 7) == 0) continue;
            if (in_list(DEFAULT_OFF, ts_key) && strcmp(ts_key, platform) != 0)
                continue;
            const toolset_def_t *d = toolsets_get_static(ts_key);
            if (!d) continue;
            if (d->includes && d->includes[0]) continue; /* composite */
            if (d->posture) continue;                    /* posture (coding) */
            size_t tn = 0;
            char **tt = toolsets_resolve(ts_key, false, &tn); /* static */
            bool ok = tn > 0 && subset_of(tt, tn, &platform_universe) &&
                      !subset_of(tt, tn, &configurable_universe) &&
                      !subset_of(tt, tn, &claimed);
            if (ok) {
                ps_add(enabled, ts_key);
                for (size_t j = 0; j < tn; j++) ps_add(&claimed, tt[j]);
            }
            toolsets_free_list(tt, tn);
        }
        toolsets_free_list(all_names, nn);
    }
    ps_free(&platform_universe);
    ps_free(&configurable_universe);
    ps_free(&claimed);

    /* ── Context-engine tools ────────────────────────────────────────── */
    {
        const json_t *ctx = config ? json_obj_get(config, "context") : NULL;
        const char *engine = "compressor";
        if (ctx && ctx->type == JSON_OBJECT) {
            const char *e = json_get_str(ctx, "engine", NULL);
            if (e && e[0]) engine = e;
        }
        /* strip + lower */
        char eng[64];
        size_t k = 0;
        for (const char *p = engine; *p && k + 1 < sizeof(eng); p++) {
            if (*p == ' ' || *p == '\t') continue;
            eng[k++] = (char)((*p >= 'A' && *p <= 'Z') ? *p + 32 : *p);
        }
        eng[k] = '\0';
        if (eng[0] && strcmp(eng, "compressor") != 0 && !explicit_empty_selection)
            ps_add(enabled, "context_engine");
    }

    /* ── Explicit passthrough + MCP ──────────────────────────────────── */
    size_t n_mcp = 0;
    char **mcp = platform_tools_enabled_mcp_server_names(config, &n_mcp);
    pset_t mcp_set = {0};
    for (size_t i = 0; i < n_mcp; i++) ps_add(&mcp_set, mcp[i]);

    pset_t passthrough = {0};
    for (size_t i = 0; i < n_names; i++) {
        const char *ts = toolset_names[i];
        if (in_list(CONFIGURABLE_KEYS, ts)) continue;
        bool is_plat_default = false;
        for (size_t j = 0; PLATFORM_ROWS[j].key; j++)
            if (strcmp(PLATFORM_ROWS[j].default_toolset, ts) == 0) { is_plat_default = true; break; }
        if (is_plat_default) continue;
        ps_add(&passthrough, ts);
    }
    bool no_mcp = false;
    for (size_t i = 0; i < n_names; i++)
        if (strcmp(toolset_names[i], "no_mcp") == 0) { no_mcp = true; break; }

    pset_t explicit_mcp = {0};
    if (no_mcp) {
        for (size_t i = 0; i < passthrough.n; i++) {
            const char *ts = passthrough.v[i];
            if (!ps_has(&mcp_set, ts) && strcmp(ts, "no_mcp") != 0)
                ps_add(enabled, ts);
        }
    } else {
        for (size_t i = 0; i < passthrough.n; i++) {
            const char *ts = passthrough.v[i];
            if (ps_has(&mcp_set, ts)) ps_add(&explicit_mcp, ts);
            else ps_add(enabled, ts);
        }
    }
    if (include_default_mcp_servers) {
        if (explicit_mcp.n > 0 || no_mcp) {
            for (size_t i = 0; i < explicit_mcp.n; i++)
                ps_add(enabled, explicit_mcp.v[i]);
        } else {
            for (size_t i = 0; i < mcp_set.n; i++)
                ps_add(enabled, mcp_set.v[i]);
        }
    } else {
        for (size_t i = 0; i < explicit_mcp.n; i++)
            ps_add(enabled, explicit_mcp.v[i]);
    }
    ps_free(&explicit_mcp);
    ps_free(&passthrough);
    ps_free(&mcp_set);
    platform_tools_free_list(mcp, n_mcp);

    /* ── agent.disabled_toolsets final override ──────────────────────── */
    {
        const json_t *agent = config ? json_obj_get(config, "agent") : NULL;
        const json_t *dis = agent ? json_obj_get(agent, "disabled_toolsets") : NULL;
        if (dis && dis->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(dis); i++) {
                char *s = json_scalar_to_str(json_get(dis, i));
                if (s) { ps_del(enabled, s); free(s); }
            }
        }
    }

    return ps_finish(enabled, out_n);
}

/* PoP: platform_tools_get @ hermes_cli/tools_config.py:_get_platform_tools */
char **platform_tools_get(const json_t *config, const char *platform,
                          bool include_default_mcp_servers, size_t *out_n) {
    pset_t enabled = {0};
    size_t n_names = 0;
    bool explicitly_configured = false;
    char **toolset_names = saved_toolset_names(config, platform, &n_names,
                                               &explicitly_configured);
    bool explicit_empty_selection = explicitly_configured && n_names == 0;

    if (!explicitly_configured) {
        const char *def = platform_tools_default_toolset(platform);
        char fallback[128];
        if (!def) {
            snprintf(fallback, sizeof(fallback), "hermes-%s", platform);
            def = fallback;
        }
        toolset_names = calloc(1, sizeof(char *));
        toolset_names[0] = strdup(def);
        n_names = 1;
    }

    bool has_explicit_config = false;
    for (size_t i = 0; i < n_names; i++)
        if (in_list(CONFIGURABLE_KEYS, toolset_names[i])) { has_explicit_config = true; break; }

    if (has_explicit_config) {
        /* direct membership of configurable keys */
        for (size_t i = 0; i < n_names; i++) {
            if (in_list(CONFIGURABLE_KEYS, toolset_names[i]) &&
                toolset_allowed_for_platform(toolset_names[i], platform))
                ps_add(&enabled, toolset_names[i]);
        }
        /* mixed config: composite names alongside configurables */
        pset_t composite_tools = {0};
        for (size_t i = 0; i < n_names; i++) {
            const char *ts = toolset_names[i];
            if (in_list(CONFIGURABLE_KEYS, ts)) continue;
            if (!toolsets_get_static(ts)) continue; /* not in TOOLSETS */
            size_t rn = 0;
            char **rt = toolsets_resolve(ts, true, &rn);
            for (size_t j = 0; j < rn; j++) ps_add(&composite_tools, rt[j]);
            toolsets_free_list(rt, rn);
        }
        if (composite_tools.n > 0) {
            pset_t expanded = {0};
            for (size_t i = 0; CONFIGURABLE_KEYS[i]; i++) {
                const char *ck = CONFIGURABLE_KEYS[i];
                if (!toolset_allowed_for_platform(ck, platform)) continue;
                size_t tn = 0;
                char **tt = toolsets_resolve(ck, false, &tn); /* static view */
                if (tn > 0 && subset_of(tt, tn, &composite_tools))
                    ps_add(&expanded, ck);
                toolsets_free_list(tt, tn);
            }
            /* default_off subtraction on the implicit expansion only */
            pset_t default_off = {0};
            for (size_t i = 0; DEFAULT_OFF[i]; i++) ps_add(&default_off, DEFAULT_OFF[i]);
            bool plat_restricted =
                strcmp(platform, "discord") == 0; /* only restricted platform */
            if (ps_has(&default_off, platform) && !plat_restricted)
                ps_del(&default_off, platform);
            {
                char *hass = config_py_get_env_value_prefer_dotenv("HASS_TOKEN");
                if (hass && hass[0]) ps_del(&default_off, "homeassistant");
                free(hass);
            }
            exempt_explicit_platform_native(&default_off, platform,
                                            explicitly_configured);
            for (size_t i = 0; i < default_off.n; i++)
                ps_del(&expanded, default_off.v[i]);
            ps_free(&default_off);
            for (size_t i = 0; i < expanded.n; i++) ps_add(&enabled, expanded.v[i]);
            ps_free(&expanded);
        }
        ps_free(&composite_tools);
    } else {
        /* subset inference from composite resolution */
        pset_t all_tool_names = {0};
        for (size_t i = 0; i < n_names; i++) {
            size_t rn = 0;
            char **rt = toolsets_resolve(toolset_names[i], true, &rn);
            for (size_t j = 0; j < rn; j++) ps_add(&all_tool_names, rt[j]);
            toolsets_free_list(rt, rn);
        }
        for (size_t i = 0; CONFIGURABLE_KEYS[i]; i++) {
            const char *ck = CONFIGURABLE_KEYS[i];
            if (!toolset_allowed_for_platform(ck, platform)) continue;
            size_t tn = 0;
            char **tt = toolsets_resolve(ck, false, &tn); /* static view */
            if (tn > 0 && subset_of(tt, tn, &all_tool_names))
                ps_add(&enabled, ck);
            toolsets_free_list(tt, tn);
        }
        bool x_search_auto = toolset_allowed_for_platform("x_search", platform) &&
                             xai_credentials_present();
        if (x_search_auto) ps_add(&enabled, "x_search");

        pset_t default_off = {0};
        for (size_t i = 0; DEFAULT_OFF[i]; i++) ps_add(&default_off, DEFAULT_OFF[i]);
        bool plat_restricted = strcmp(platform, "discord") == 0;
        if (ps_has(&default_off, platform) && !plat_restricted)
            ps_del(&default_off, platform);
        {
            char *hass = config_py_get_env_value_prefer_dotenv("HASS_TOKEN");
            if (hass && hass[0]) ps_del(&default_off, "homeassistant");
            free(hass);
        }
        if (x_search_auto) ps_del(&default_off, "x_search");
        exempt_explicit_platform_native(&default_off, platform,
                                        explicitly_configured);
        for (size_t i = 0; i < default_off.n; i++)
            ps_del(&enabled, default_off.v[i]);
        ps_free(&default_off);
        ps_free(&all_tool_names);
    }

    /* recovery + plugin + context-engine + passthrough + MCP + disabled */
    char **result = platform_tools_get_finish(config, platform,
                                              include_default_mcp_servers,
                                              &enabled, toolset_names, n_names,
                                              explicit_empty_selection, out_n);
    for (size_t i = 0; i < n_names; i++) free(toolset_names[i]);
    free(toolset_names);
    (void)explicit_empty_selection;
    return result;
}

/* PoP: platform_tools_summary @ hermes_cli/tools_config.py:_platform_toolset_summary */
json_t *platform_tools_summary(const json_t *config,
                               const char *const *platforms, size_t n_platforms) {
    char **detected = NULL;
    size_t nd = 0;
    if (!platforms) {
        detected = platform_tools_get_enabled_platforms(&nd);
        platforms = (const char *const *)detected;
        n_platforms = nd;
    }
    json_t *out = json_object();
    for (size_t i = 0; i < n_platforms; i++) {
        size_t tn = 0;
        char **ts = platform_tools_get(config, platforms[i], true, &tn);
        json_t *arr = json_array();
        for (size_t j = 0; j < tn; j++)
            json_append(arr, json_string(ts[j]));
        platform_tools_free_list(ts, tn);
        json_set(out, platforms[i], arr);
    }
    if (detected) platform_tools_free_list(detected, nd);
    return out;
}

/* PoP: platform_tools_save @ hermes_cli/tools_config.py:_save_platform_tools */
void platform_tools_save(json_t *config, const char *platform,
                         const char *const *enabled_keys, size_t n_enabled) {
    if (!config || !platform) return;
    json_t *pt = json_obj_get(config, "platform_toolsets");
    if (!pt || pt->type != JSON_OBJECT) {
        pt = json_object();
        json_set(config, "platform_toolsets", pt);
    }

    /* drop platform-scoped toolsets that don't apply here */
    pset_t enabled = {0};
    for (size_t i = 0; i < n_enabled; i++)
        if (toolset_allowed_for_platform(enabled_keys[i], platform))
            ps_add(&enabled, enabled_keys[i]);

    /* preserved entries: existing minus configurable minus platform-defaults */
    pset_t preserved = {0};
    const json_t *existing = json_obj_get(pt, platform);
    if (existing && existing->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(existing); i++) {
            char *s = json_scalar_to_str(json_get(existing, i));
            if (!s) continue;
            bool is_plat_default = false;
            for (size_t j = 0; PLATFORM_ROWS[j].key; j++)
                if (strcmp(PLATFORM_ROWS[j].default_toolset, s) == 0) { is_plat_default = true; break; }
            if (!in_list(CONFIGURABLE_KEYS, s) && !is_plat_default)
                ps_add(&preserved, s);
            free(s);
        }
    }
    ps_del(&preserved, "no_mcp"); /* saving from picker clears the sentinel */

    /* union → sorted array */
    pset_t merged = {0};
    for (size_t i = 0; i < enabled.n; i++) ps_add(&merged, enabled.v[i]);
    for (size_t i = 0; i < preserved.n; i++) ps_add(&merged, preserved.v[i]);
    size_t mn = 0;
    char **mv = ps_finish(&merged, &mn);
    json_t *arr = json_array();
    for (size_t i = 0; i < mn; i++) json_append(arr, json_string(mv[i]));
    json_set(pt, platform, arr);
    platform_tools_free_list(mv, mn);

    /* reconcile agent.disabled_toolsets: clear entries the user just
     * explicitly enabled for this platform */
    json_t *agent = json_obj_get(config, "agent");
    if (agent && agent->type == JSON_OBJECT) {
        json_t *dis = json_obj_get(agent, "disabled_toolsets");
        if (dis && dis->type == JSON_ARRAY && json_len(dis) > 0 && enabled.n > 0) {
            json_t *remaining = json_array();
            bool changed = false;
            for (size_t i = 0; i < json_len(dis); i++) {
                char *s = json_scalar_to_str(json_get(dis, i));
                bool newly_enabled = s && ps_has(&enabled, s) && !ps_has(&preserved, s);
                if (newly_enabled) changed = true;
                else if (s) json_append(remaining, json_string(s));
                free(s);
            }
            if (changed) json_set(agent, "disabled_toolsets", remaining);
            else json_free(remaining);
        }
    }
    ps_free(&enabled);
    ps_free(&preserved);
}
/* MARKER-PART2 */
